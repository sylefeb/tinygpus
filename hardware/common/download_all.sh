#!/bin/bash
SILICE_URL=https://raw.githubusercontent.com/sylefeb/Silice/master/
MCH2022_SILICE_URL=https://raw.githubusercontent.com/sylefeb/mch2022-silice/main/
OPT=-nc
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_25.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_50.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_50_lock.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_60.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_66_lock.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_66.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_70.v
wget $OPT $SILICE_URL/projects/common/plls/icebrkr_100.v
wget $OPT $SILICE_URL/projects/common/ice40_sb_io.v
wget $OPT $SILICE_URL/projects/common/ice40_sb_io_ddr.v
wget $OPT $SILICE_URL/projects/common/ice40_sb_io_inout.v
wget $OPT $SILICE_URL/projects/common/ddr.v
wget $OPT $SILICE_URL/projects/common/ice40_spram.v
wget $OPT $SILICE_URL/projects/common/simulation_spram.si
wget $OPT $SILICE_URL/projects/common/clean_reset.si
wget $OPT $SILICE_URL/projects/common/ice40_half_quarter_clock.v
wget $OPT $SILICE_URL/projects/common/ice40_half_clock.v
wget $OPT $SILICE_URL/projects/common/divint_std.si
wget $OPT $SILICE_URL/projects/common/uart.si
wget $OPT $SILICE_URL/projects/common/verilator_data.v
wget $OPT $SILICE_URL/projects/spiflash/spiflash2x.si
wget $OPT $SILICE_URL/projects/spiflash/ddr_clock.v
wget $OPT $SILICE_URL/projects/ice-v/CPUs/ice-v-dual.si
wget $OPT $MCH2022_SILICE_URL/common/qpsram2x.si
