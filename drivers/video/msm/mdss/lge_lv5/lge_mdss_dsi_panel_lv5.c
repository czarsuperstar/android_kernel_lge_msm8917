#include <linux/delay.h>
#include "mdss_dsi.h"
#include "mdss_mdp.h"
#include "lge/reader_mode.h"
#include "lge_mdss_dsi_panel_lv5.h"
#include <linux/mfd/dw8768.h>

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
#include <soc/qcom/lge/board_lge.h>
#endif

#if 0 //IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);
#endif

#if 0
extern int mfts_lpwg_on;
#endif

int set_touch_osc(int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	char name[32];
	int i, index = -1;

	if (!lgd_lg4894_pdata_base) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ctrl = container_of(lgd_lg4894_pdata_base, struct mdss_dsi_ctrl_pdata,
					panel_data);
	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		strcpy(name, "touch-osc-on");
	} else {
		strcpy(name, "touch-osc-off");
	}

	for (i = 0; i < ctrl->lge_extra.num_extra_cmds; ++i) {
		if (!strcmp(ctrl->lge_extra.extra_cmds_array[i].name, name)) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		pr_err("%s: no touch ocs on/off cmd\n", __func__);
		return -EINVAL;
	}

	if (ctrl->lge_extra.extra_cmds_array[index].cmds.cmd_cnt) {
		mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

		if (lgd_lg4894_pdata_base->panel_info.panel_power_state == MDSS_PANEL_POWER_OFF) {
			ctrl->lge_extra.extra_cmds_array[index].cmds.link_state = DSI_LP_MODE;
			mdss_dsi_sw_reset(ctrl, true);
		} else {
			ctrl->lge_extra.extra_cmds_array[index].cmds.link_state = DSI_HS_MODE;
		}

		lge_mdss_dsi_panel_extra_cmds_send(ctrl, name);
		pr_info("%s:enable=%d\n", __func__, enable);

		mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	}

	if (enable) {
		dw8768_register_set(0x05, 0x0F);
		dw8768_register_set(0x03, 0x83);
		dw8768_register_set(0x00, 0x0D);
		dw8768_register_set(0x01, 0x14);
	} else {
		dw8768_register_set(0x03, 0x80);
		dw8768_register_set(0x05, 0x07);
	}

	return 0;
}
EXPORT_SYMBOL(set_touch_osc);

#if 0 //IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: (override: ph2_global_com)\n", __func__);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_UNIFIED_DRIVER_3) || IS_ENABLED(CONFIG_LGE_TOUCH_CORE)
	if(lge_get_mfts_mode()) {
			if (!mfts_lpwg_on) {
				if(lge_get_panel_type() == PH2_SHARP) {
					lge_extra_gpio_set_value(ctrl_pdata, "touch-reset", 0);
					LGE_MDELAY(2);

					lge_extra_gpio_set_value(ctrl_pdata, "touch-avdd", 0);
					LGE_MDELAY(2);
					lge_extra_gpio_set_value(ctrl_pdata, "touch-vdddc", 0);
					lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
				} else if(lge_get_panel_type() == PH2_JDI) {
					LGE_MDELAY(1);
					lge_extra_gpio_set_value(ctrl_pdata, "touch-reset", 0);
					lge_extra_gpio_set_value(ctrl_pdata, "touch-vdddc", 0);
					lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
					LGE_MDELAY(1);
					lge_extra_gpio_set_value(ctrl_pdata, "touch-avdd", 0);
				}
			}
	}
#endif

	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

end:
	return ret;
}
#endif

#if 0 //IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: (override: ph2_global_com)\n", __func__);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_UNIFIED_DRIVER_3) || IS_ENABLED(CONFIG_LGE_TOUCH_CORE)
		if(lge_get_mfts_mode()) {
				if (!mfts_lpwg_on) {
					if(lge_get_panel_type() == PH2_SHARP) {
						lge_extra_gpio_set_value(ctrl_pdata, "touch-reset", 0);
						lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
						lge_extra_gpio_set_value(ctrl_pdata, "touch-avdd", 1);
						LGE_MDELAY(2);
						lge_extra_gpio_set_value(ctrl_pdata, "touch-vdddc", 1);
						LGE_MDELAY(25);
					} else if(lge_get_panel_type() == PH2_JDI) {
						lge_extra_gpio_set_value(ctrl_pdata, "touch-reset", 0);
						lge_extra_gpio_set_value(ctrl_pdata, "touch-avdd", 1);
						LGE_MDELAY(1);
						lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
						lge_extra_gpio_set_value(ctrl_pdata, "touch-reset", 1);
						lge_extra_gpio_set_value(ctrl_pdata, "touch-vdddc", 1);
						LGE_MDELAY(1);
					}
				}
		}
#endif

	ret = msm_dss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}

	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_READER_MODE)
static struct dsi_panel_cmds reader_mode_cmds[4];

int lge_mdss_dsi_parse_reader_mode_cmds(struct device_node *np, struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_OFF],
        "qcom,panel-reader-mode-off-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_1],
        "qcom,panel-reader-mode-step1-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_2],
        "qcom,panel-reader-mode-step2-command", "qcom,mdss-dsi-reader-mode-command-state");
	mdss_dsi_parse_dcs_cmds(np, &reader_mode_cmds[READER_MODE_STEP_3],
        "qcom,panel-reader-mode-step3-command", "qcom,mdss-dsi-reader-mode-command-state");

	return 0;
}

static bool change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int new_mode)
{
	if (new_mode == READER_MODE_MONO) {
		pr_info("%s: READER_MODE_MONO is not supported. reader mode is going off.\n", __func__);
		new_mode = READER_MODE_STEP_2;
	}

	if(reader_mode_cmds[new_mode].cmd_cnt) {
		pr_info("%s: sending reader mode commands [%d]\n", __func__, new_mode);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		mdss_dsi_panel_cmds_send(ctrl, &reader_mode_cmds[new_mode], CMD_REQ_COMMIT);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}
	return true;
}

bool lge_change_reader_mode(struct mdss_dsi_ctrl_pdata *ctrl, int old_mode, int new_mode)
{
	if (old_mode == new_mode) {
		pr_info("%s: same mode [%d]\n", __func__, new_mode);
		return true;
	}

	return change_reader_mode(ctrl, new_mode);
}

int lge_mdss_dsi_panel_send_post_on_cmds(struct mdss_dsi_ctrl_pdata *ctrl, int cur_mode)
{
	if (cur_mode != READER_MODE_OFF)
		change_reader_mode(ctrl, cur_mode);
	return 0;
}
#endif
