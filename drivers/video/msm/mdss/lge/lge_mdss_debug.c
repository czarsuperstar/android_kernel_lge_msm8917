
#include "lge_mdss_debug.h"


#define TYPECAST_TO_C(in, out) 	((out) = (char)(in))
#define TYPECAST_TO_I(in, out) 	((out) = (int)(in))
#define TYPECAST_TO_UI(in, out) ((out) = (u32)(in))



struct debug_file_info *debug_finfo;
struct dsi_panel_cmds dsi_access_cmds;

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds, u32 flags);


static int op_flag = 0;


static int lge_get_file_size(char *file_name)
{
	struct path p;
	struct kstat ks;

	if (kern_path(file_name, 0, &p)){
		return 0;
	} else {
		vfs_getattr(&p, &ks);
		pr_info("%s: file size [%d] \n", __func__, (int)ks.size);
	}

	return ks.size;
}

static void lge_select_data_type(struct debug_file_info *debug_finfo)
{
	switch(debug_finfo->event) { //NOTE : choose which output-data-format(hex or dec) you are going to use
	case DEBUG_DSI_CMD_TX:
		debug_finfo->data_type = 16;
		break;
	case DEBUG_PWR_SEQ_DELAY:
	case DEBUG_BLMAP_CHANGE:
	case DEBUG_DSI_TIMING_CHANGE:
		debug_finfo->data_type = 10;
		break;
	default:
		debug_finfo->data_type = 16;
		break;
	}
}

static void lge_change_data_type(struct debug_file_info *debug_finfo)
{
	switch(debug_finfo->event) { //NOTE : during reading data, change data type to be parsed if needs
	case DEBUG_DSI_CMD_TX:
	case DEBUG_PWR_SEQ_DELAY:
	case DEBUG_BLMAP_CHANGE:
		break;
	case DEBUG_DSI_TIMING_CHANGE:
		if(debug_finfo->data_len == 6)
			debug_finfo->data_type = 16;
		break;
	default:
		break;
	}
}

#define MAX_PARSED_VAL_LEN 10

static int lge_parse_data_from_file(struct debug_file_info *debug_finfo)
{
	int ret = 0;
	struct file *p_fp, *fp = NULL;
	loff_t pos = 0;
	int fd;
	int i, cnt;
	char buf[MAX_PARSED_VAL_LEN+1];
	unsigned long val;
	int *p_data;

	mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

	if (debug_finfo == NULL) {
		pr_err("%s: invalid debug file info \n", __func__);
		ret = -EINVAL;
		goto error;
	}

	debug_finfo->data_len = 0;
	p_data = debug_finfo->ibuf;

	fd = sys_open(debug_finfo->file_name, O_RDONLY, 0);
	if (fd >= 0) {
		fp = fget(fd);
		if (fp) {
			p_fp = fp;
			i = cnt = 0;
			do {
				vfs_read(p_fp, &buf[i], sizeof(char), &pos);
				pr_debug("%s: %d-buf[%d] : %d \n", __func__, cnt, i, buf[i]);
				if ((buf[i]>='0' && buf[i]<='9') || (buf[i]>='a' && buf[i]<='f') || (buf[i]>='A' && buf[i]<='F')) {
					if(i <= MAX_PARSED_VAL_LEN) {
						i++;
						goto next;
					} else {
						pr_err("%s: parsed value's length is out of range!! \n", __func__);
						ret = -EINVAL;
						goto error;
					}
				} else {
					buf[i] = '\0';
					if (kstrtoul(buf, debug_finfo->data_type, &val) < 0) {
						pr_debug("%s: failed to convert data #%d!! \n", __func__, debug_finfo->data_len);
					} else {
						*p_data = (int)val;
						pr_debug("%s: 0x%x[%d] \n", __func__, *p_data, *p_data);
						p_data++;
						debug_finfo->data_len++;

						lge_change_data_type(debug_finfo);
					}
					i = 0;
				}
next:
				cnt++;
			}while(cnt <= debug_finfo->file_size);
		} else {
			pr_err("%s: invalid debug file!! \n", __func__);
			ret = -EINVAL;
			goto error;
		}
	} else {
		pr_err("%s: no debug file to open!! [error:%d] \n", __func__, fd);
		ret = -EINVAL;
		goto error;
	}

	pr_info("%s: data_len[%d] \n", __func__, debug_finfo->data_len);

error:
	sys_close(fd);

	set_fs(old_fs);

	return ret;
}

static int lge_parse_dsi_cmds(struct debug_file_info *debug_finfo, struct dsi_panel_cmds *pcmds)
{
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	if (debug_finfo == NULL) {
		pr_err("%s: invalid debug file info \n", __func__);
		return -EINVAL;
	}

	buf = debug_finfo->cbuf;
	blen = debug_finfo->data_len;

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			goto exit_free;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		goto exit_free;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	/*Set default link state to HS Mode*/
	pcmds->link_state = DSI_HS_MODE;

	pr_info("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	return -EINVAL;

}

static int lge_read_dsi_status(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds)
{
	int i, j, rc;
	struct dcs_cmd_req cmdreq;

	rc = 1;

	for (i = 0; i < pcmds->cmd_cnt; ++i) {
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = pcmds->cmds + i;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_RX | CMD_REQ_HS_MODE;
		cmdreq.rlen = pcmds->cmds[i].dchdr.dlen - 1;
		cmdreq.cb = NULL;
		cmdreq.rbuf = ctrl->status_buf.data;

		cmdreq.flags |= CMD_REQ_HS_MODE;

		rc = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
		if (rc <= 0) {
			pr_err("%s: get status: fail\n", __func__);
			return rc;
		}

		pr_info("%s: read dsi parameters for 0x%x :", __func__, pcmds->cmds[i].payload[0]);
		for(j=0; j<pcmds->cmds[i].dchdr.dlen - 1; j++)
			pr_info("%s: 0x%x", __func__, ctrl->status_buf.data[j]);
	}

	return rc;
}

static int lge_apply_parsed_data(struct mdss_panel_data *pdata, struct mdss_dsi_ctrl_pdata *ctrl, struct mdss_panel_info *pinfo,
	struct debug_file_info *debug_finfo)
{
	int ret = 0;
	int i;

	if (pdata == NULL) {
		pr_err("%s: invalid pdata \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	if (ctrl == NULL) {
		pr_err("%s: invalid ctrl_pdata \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	if (pinfo == NULL) {
		pr_err("%s: invalid panel_info \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	if (debug_finfo == NULL) {
		pr_err("%s: invalid debug file info \n", __func__);
		ret = -EINVAL;
		goto finish;
	}


	//NOTE : use parsed integer buffer for whatever you want to use by type-casting
	switch(debug_finfo->event) {
	case DEBUG_DSI_CMD_TX:
		debug_finfo->cbuf = kzalloc(sizeof(char) * debug_finfo->data_len, GFP_KERNEL);
		if (debug_finfo->cbuf == NULL) {
			pr_err("%s: allocation failed for debug_finfo->data!! \n", __func__);
			ret = -ENOMEM;
			goto error;
		}

		for(i=0; i<debug_finfo->data_len; i++)
			TYPECAST_TO_C(debug_finfo->ibuf[i], debug_finfo->cbuf[i]);

		ret = lge_parse_dsi_cmds(debug_finfo, &dsi_access_cmds);
		if (ret < 0) {
			goto error;
		}

		mdss_dsi_panel_cmds_send(ctrl, &dsi_access_cmds, CMD_REQ_COMMIT);
		break;
	case DEBUG_DSI_CMD_RX:
		debug_finfo->cbuf = kzalloc(sizeof(char) * debug_finfo->data_len, GFP_KERNEL);
		if (debug_finfo->cbuf == NULL) {
			pr_err("%s: allocation failed for debug_finfo->data!! \n", __func__);
			ret = -ENOMEM;
			goto error;
		}

		for(i=0; i<debug_finfo->data_len; i++)
			TYPECAST_TO_C(debug_finfo->ibuf[i], debug_finfo->cbuf[i]);

		ret = lge_parse_dsi_cmds(debug_finfo, &dsi_access_cmds);
		if (ret < 0) {
			goto error;
		}

		lge_read_dsi_status(ctrl, &dsi_access_cmds);
		break;
	case DEBUG_PWR_SEQ_DELAY:
		if (debug_finfo->data_len > 10) {
			pr_err("%s: invalid # of parsed delay data!! \n", __func__);
			ret = -EINVAL;
			goto error;
		}

		for(i=0; i<debug_finfo->data_len; i++)
			ctrl->debug_pwr_seq_dly[i] = debug_finfo->ibuf[i];
		break;
	case DEBUG_PWR_ALWAYS_ON:
		if (debug_finfo->data_len > 10) {
			pr_err("%s: invalid # of parsed pwr-always-on data!! \n", __func__);
			ret = -EINVAL;
			goto error;
		}

		for(i=0; i<debug_finfo->data_len; i++)
			ctrl->debug_pwr_always_on[i] = debug_finfo->ibuf[i];
		break;
	case DEBUG_BLMAP_CHANGE:
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_USE_BLMAP)
		if (debug_finfo->data_len != 256) {
			pr_err("%s: invalid # of parsed blmap data!! \n", __func__);
			ret = -EINVAL;
			goto error;
		}

		for(i=0; i<debug_finfo->data_len; i++)
			pinfo->blmap[i] = debug_finfo->ibuf[i];
#else
		pr_info("%s: CONFIG_LGE_DISPLAY_BL_USE_BLMAP is not enabled \n", __func__);
#endif
		break;
	case DEBUG_DSI_TIMING_CHANGE:
		if (debug_finfo->data_len != 6+sizeof(pinfo->mipi.dsi_phy_db.timing)+2) {
			pr_err("%s: invalid # of parsed dsi timing data!! \n", __func__);
			ret = -EINVAL;
			goto error;
		}

		TYPECAST_TO_UI(debug_finfo->ibuf[0], pinfo->lcdc.h_front_porch);
		TYPECAST_TO_UI(debug_finfo->ibuf[1], pinfo->lcdc.h_back_porch);
		TYPECAST_TO_UI(debug_finfo->ibuf[2], pinfo->lcdc.h_pulse_width);
		TYPECAST_TO_UI(debug_finfo->ibuf[3], pinfo->lcdc.v_front_porch);
		TYPECAST_TO_UI(debug_finfo->ibuf[4], pinfo->lcdc.v_back_porch);
		TYPECAST_TO_UI(debug_finfo->ibuf[5], pinfo->lcdc.v_pulse_width);
		pr_info("%s: changed porch[%d %d %d %d %d %d] \n", __func__,
			pinfo->lcdc.h_front_porch, pinfo->lcdc.h_back_porch, pinfo->lcdc.h_pulse_width,
			pinfo->lcdc.v_front_porch, pinfo->lcdc.v_back_porch, pinfo->lcdc.v_pulse_width);

		for(i=0; i<sizeof(pinfo->mipi.dsi_phy_db.timing); i++) {
			TYPECAST_TO_C(debug_finfo->ibuf[6+i], pinfo->mipi.dsi_phy_db.timing[i]);
			pr_info("%s: changed dsi timing[0x%x] \n", __func__, pinfo->mipi.dsi_phy_db.timing[i]);
		}

		TYPECAST_TO_C(debug_finfo->ibuf[6+sizeof(pinfo->mipi.dsi_phy_db.timing)], pinfo->mipi.t_clk_post);
		TYPECAST_TO_C(debug_finfo->ibuf[6+sizeof(pinfo->mipi.dsi_phy_db.timing)+1], pinfo->mipi.t_clk_pre);
		pr_info("%s: changed t_clk[0x%x 0x%x] \n", __func__, pinfo->mipi.t_clk_post, pinfo->mipi.t_clk_pre);
		break;
	default:
		break;
	}

error:
finish:
	return ret;
}

int lge_debug_event_trigger(struct mdss_panel_data *pdata, char *debug_file, int debug_event)
{
	int ret = 0;
	loff_t size = 0;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct mdss_panel_info *pinfo = NULL;


	if (pdata == NULL) {
		pr_err("%s: invalid pdata \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (ctrl == NULL) {
		pr_err("%s: invalid ctrl_pdata \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("%s: invalid panel_info \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	if (!op_flag) { //NOTE : JUST IN CASE, do not operate this in first unblank
		if (pinfo->panel_power_state == MDSS_PANEL_POWER_ON) {
			pr_info("%s: not operated for first unblank \n", __func__);
			return ret;
		} else {
			op_flag = 1;
		}
	}

	pr_info("%s: start event[%d] \n", __func__, debug_event);

	size = lge_get_file_size(debug_file);
	if (size == 0) {
		pr_err("%s: file is null \n", __func__);
		ret = -EINVAL;
		goto finish;
	}

	debug_finfo = kzalloc(sizeof(struct debug_file_info), GFP_KERNEL);
	if (debug_finfo == NULL) {
		pr_err("%s: allocation failed for debug_finfo!! \n", __func__);
		ret = -ENOMEM;
		goto finish;
	}

	strcpy(debug_finfo->file_name, debug_file);
	debug_finfo->file_size = size;
	debug_finfo->event = debug_event;

	lge_select_data_type(debug_finfo);

	debug_finfo->ibuf = kzalloc(sizeof(int) * debug_finfo->file_size, GFP_KERNEL);
	if (debug_finfo->ibuf == NULL) {
		pr_err("%s: allocation failed for debug_finfo->data!! \n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	ret = lge_parse_data_from_file(debug_finfo);
	if (ret < 0)
		goto error;

	ret = lge_apply_parsed_data(pdata, ctrl, pinfo, debug_finfo);
	if (ret < 0)
		goto error;

error:
	if (debug_finfo->cbuf)
		kfree(debug_finfo->cbuf);
	if (debug_finfo->ibuf)
		kfree(debug_finfo->ibuf);
	if (debug_finfo)
		kfree(debug_finfo);

finish:
	pr_info("%s: end \n", __func__);

	return ret;
}
