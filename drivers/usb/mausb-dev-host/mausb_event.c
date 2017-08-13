/*
 * Copyright (C) 2015-2016 Nagaraju Kadiri
 *
 * mausb_event.c
 *
 */

#include <linux/kthread.h>
#include <linux/export.h>

#include "mausb_common.h"
#include "mausb_util.h"

static int event_handler(struct mausb_device *ud)
{
	mausb_dbg_eh("---> event_handler\n");

	/*
	 * Events are handled by only this thread.
	 */
	while (mausb_event_happened(ud)) {
		mausb_dbg_eh("pending event %lx\n", ud->event);

		/*
		 * NOTE: shutdown must come first.
		 * Shutdown the device.
		 */
		if (ud->event & MAUSB_EH_SHUTDOWN) {
			ud->mausb_eh_ops.shutdown(ud);
			ud->event &= ~MAUSB_EH_SHUTDOWN;
		}

		/* Reset the device. */
		if (ud->event & MAUSB_EH_RESET) {
			ud->mausb_eh_ops.reset(ud);
			ud->event &= ~MAUSB_EH_RESET;
		}

		/* Mark the device as unusable. */
		if (ud->event & MAUSB_EH_UNUSABLE) {
			ud->mausb_eh_ops.unusable(ud);
			ud->event &= ~MAUSB_EH_UNUSABLE;
		}

		/* Stop the error handler. */
		if (ud->event & MAUSB_EH_BYE)
			return -1;
	}

	return 0;
}

static int event_handler_loop(void *data)
{
	struct mausb_device *ud = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(ud->eh_waitq,
					 mausb_event_happened(ud) ||
					 kthread_should_stop());
		

		if (event_handler(ud) < 0)
			break;
	}

	return 0;
}

int mausb_start_eh(struct mausb_device *ud)
{
	init_waitqueue_head(&ud->eh_waitq);
	ud->event = 0;

	ud->eh = kthread_run(event_handler_loop, ud, "mausb_eh");
	if (IS_ERR(ud->eh)) {
		return PTR_ERR(ud->eh);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mausb_start_eh);

void mausb_stop_eh(struct mausb_device *ud)
{
	if (ud->eh == current)
		return; /* do not wait for myself */

	kthread_stop(ud->eh);
	mausb_dbg_eh("mausb_eh has finished\n");
}
EXPORT_SYMBOL_GPL(mausb_stop_eh);

void mausb_event_add(struct mausb_device *ud, unsigned long event)
{
	unsigned long flags;
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,"\n ---> mausb_event_add");
	spin_lock_irqsave(&ud->lock, flags);
	ud->event |= event;
	wake_up(&ud->eh_waitq);
	spin_unlock_irqrestore(&ud->lock, flags);
	LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,"\n <-- mausb_event_add");
}
EXPORT_SYMBOL_GPL(mausb_event_add);

int mausb_event_happened(struct mausb_device *ud)
{
	int happened = 0;
	unsigned long flags;
	spin_lock_irqsave(&ud->lock, flags);
	if (ud->event != 0) {
		LG_PRINT(DBG_LEVEL_MEDIUM,DATA_TRANS_MAIN,"mausb_event_happend : %lu",ud->event);
		happened = 1;
	}
	spin_unlock_irqrestore(&ud->lock, flags);

	return happened;
}
EXPORT_SYMBOL_GPL(mausb_event_happened);
