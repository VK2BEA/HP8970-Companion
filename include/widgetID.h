/*
 * widgetID.h
 *
 *  Created on: Aug 12, 2024
 *      Author: michael
 */

#ifndef WIDGETID_H_
#define WIDGETID_H_

typedef enum {
    eW_aspect_Plot,
    eW_box_Spot,
    eW_btn_Calibrate,
    eW_btn_ColorReset,
    eW_btn_CSV,
    eW_btn_Memory,
    eW_btn_PDF,
    eW_btn_PNG,
    eW_btn_Print,
    eW_btn_RestoreJSON,
    eW_btn_SaveJSON,
    eW_btn_SettingsDelete,
    eW_btn_SettingsRestore,
    eW_btn_SettingsSave,
    eW_btn_SVG,
    eW_chk_AutoScale,
    eW_chk_Correction,
    eW_chk_LossOn,
    eW_chk_Settings8970A,
    eW_chk_Settings8970B,
    eW_chk_Settings8970Bopt20,
    eW_chk_SettingsHPlogo,
    eW_chk_SettingsTime,
    eW_chk_ShowMemory,
    eW_chk_useGPIBdeviceName,
    eW_chk_use_LO_GPIBdeviceName,
    eW_color_Freq,
    eW_color_Gain,
    eW_color_Grid,
    eW_color_GridGain,
    eW_color_Noise,
    eW_color_Title,
    eW_combo_Mode,
    eW_combo_SettingsConfigurations,
    eW_Controls,
    eW_CV_NoiseSource,
    eW_drawing_Plot,
    eW_drop_InputGainCalibration,
    eW_drop_NoiseUnits,
    eW_drop_Smoothing,
    eW_entry_opt_GPIB_name,
    eW_entry_opt_LO_GPIB_name,
    eW_entry_Title,
    eW_frame_NoiseRange,
    eW_frm_Mode,
    eW_frm_Sweep,
    eW_HP8970_application,
    eW_lbl_LOnotice,
    eW_lbl_version,
    eW_lbl_Status,
    eW_lbl_Status_LO,
    eW_LO_combo_sideband,
    eW_LO_entry_LO_Freq,
    eW_LO_entry_LO_Setup,
    eW_LO_frm_FixedIF_Freq,
    eW_LO_frm_FixedLO_Freq,
    eW_LO_frm_sideband,
    eW_LO_spin_FixedIF_Freq,
    eW_LO_spin_FixedLO_Freq,
    eW_LO_spin_SettlingTime,
    eW_note_Controls,
    eW_NS_btn_Add,
    eW_NS_btn_Delete,
    eW_NS_btn_Save,
    eW_NS_btn_Upload,
    eW_NS_combo_Source,
    eW_page_GPIB,
    eW_page_Notes,
    eW_page_Options,
    eW_page_Plot,
    eW_page_Settings,
    eW_page_SigGen,
    eW_page_Source,
    eW_scroll_NoiseSource,
    eW_chk_SettingsA4,
    eW_chk_SettingsLetter,
    eW_chk_SettingsA3,
    eW_chk_SettingsTabloid,
    eW_spin_ColdT,
    eW_spin_Frequency,
    eW_spin_FrStart,
    eW_spin_FrStep_Cal,
    eW_spin_FrStep_Sweep,
    eW_spin_FrStop,
    eW_spin_GainMax,
    eW_spin_GainMin,
    eW_spin_LossAfter,
    eW_spin_LossBefore,
    eW_spin_LossT,
    eW_spin_NoiseMax,
    eW_spin_NoiseMin,
    eW_spin_opt_ControllerIdx,
    eW_spin_opt_GPIB_PID,
    eW_spin_opt_GPIB_PID_LO,
    eW_Splash,
    eW_textView_Notes,
    eW_tgl_Spot,
    eW_tgl_Sweep,
    eW_N_WIDGETS
} widgetIDs;

#endif /* WIDGETID_H_ */
