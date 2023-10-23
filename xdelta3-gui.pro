# *****************************************************************************
# * Copyright (C) 2023 MX Authors
# *
# * Authors: Adrian <adrian@mxlinux.org>
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of xdelta3-gui.
# *
# * xdelta3-gui is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * xdelta3-gui is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with xdelta3-gui.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************/

QT          += core gui widgets
CONFIG      += c++1z
TARGET      = xdelta3-gui
TEMPLATE    = app

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    cmd.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    cmd.h \
    mainwindow.h \
    version.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += translations/xdelta3-gui_af.ts \
                translations/xdelta3-gui_am.ts \
                translations/xdelta3-gui_ar.ts \
                translations/xdelta3-gui_ast.ts \
                translations/xdelta3-gui_be.ts \
                translations/xdelta3-gui_bg.ts \
                translations/xdelta3-gui_bn.ts \
                translations/xdelta3-gui_bs.ts \
                translations/xdelta3-gui_ca.ts \
                translations/xdelta3-gui_ceb.ts \
                translations/xdelta3-gui_co.ts \
                translations/xdelta3-gui_cs.ts \
                translations/xdelta3-gui_cy.ts \
                translations/xdelta3-gui_da.ts \
                translations/xdelta3-gui_de.ts \
                translations/xdelta3-gui_el.ts \
                translations/xdelta3-gui_en_GB.ts \
                translations/xdelta3-gui_en.ts \
                translations/xdelta3-gui_en_US.ts \
                translations/xdelta3-gui_eo.ts \
                translations/xdelta3-gui_es_ES.ts \
                translations/xdelta3-gui_es.ts \
                translations/xdelta3-gui_et.ts \
                translations/xdelta3-gui_eu.ts \
                translations/xdelta3-gui_fa.ts \
                translations/xdelta3-gui_fil_PH.ts \
                translations/xdelta3-gui_fil.ts \
                translations/xdelta3-gui_fi.ts \
                translations/xdelta3-gui_fr_BE.ts \
                translations/xdelta3-gui_fr.ts \
                translations/xdelta3-gui_fy.ts \
                translations/xdelta3-gui_ga.ts \
                translations/xdelta3-gui_gd.ts \
                translations/xdelta3-gui_gl_ES.ts \
                translations/xdelta3-gui_gl.ts \
                translations/xdelta3-gui_gu.ts \
                translations/xdelta3-gui_ha.ts \
                translations/xdelta3-gui_haw.ts \
                translations/xdelta3-gui_he_IL.ts \
                translations/xdelta3-gui_he.ts \
                translations/xdelta3-gui_hi.ts \
                translations/xdelta3-gui_hr.ts \
                translations/xdelta3-gui_ht.ts \
                translations/xdelta3-gui_hu.ts \
                translations/xdelta3-gui_hye.ts \
                translations/xdelta3-gui_hy.ts \
                translations/xdelta3-gui_id.ts \
                translations/xdelta3-gui_ie.ts \
                translations/xdelta3-gui_is.ts \
                translations/xdelta3-gui_it.ts \
                translations/xdelta3-gui_ja.ts \
                translations/xdelta3-gui_jv.ts \
                translations/xdelta3-gui_kab.ts \
                translations/xdelta3-gui_ka.ts \
                translations/xdelta3-gui_kk.ts \
                translations/xdelta3-gui_km.ts \
                translations/xdelta3-gui_kn.ts \
                translations/xdelta3-gui_ko.ts \
                translations/xdelta3-gui_ku.ts \
                translations/xdelta3-gui_ky.ts \
                translations/xdelta3-gui_lb.ts \
                translations/xdelta3-gui_lo.ts \
                translations/xdelta3-gui_lt.ts \
                translations/xdelta3-gui_lv.ts \
                translations/xdelta3-gui_mg.ts \
                translations/xdelta3-gui_mi.ts \
                translations/xdelta3-gui_mk.ts \
                translations/xdelta3-gui_ml.ts \
                translations/xdelta3-gui_mn.ts \
                translations/xdelta3-gui_mr.ts \
                translations/xdelta3-gui_ms.ts \
                translations/xdelta3-gui_mt.ts \
                translations/xdelta3-gui_my.ts \
                translations/xdelta3-gui_nb_NO.ts \
                translations/xdelta3-gui_nb.ts \
                translations/xdelta3-gui_ne.ts \
                translations/xdelta3-gui_nl_BE.ts \
                translations/xdelta3-gui_nl.ts \
                translations/xdelta3-gui_nn.ts \
                translations/xdelta3-gui_ny.ts \
                translations/xdelta3-gui_oc.ts \
                translations/xdelta3-gui_or.ts \
                translations/xdelta3-gui_pa.ts \
                translations/xdelta3-gui_pl.ts \
                translations/xdelta3-gui_ps.ts \
                translations/xdelta3-gui_pt_BR.ts \
                translations/xdelta3-gui_pt.ts \
                translations/xdelta3-gui_ro.ts \
                translations/xdelta3-gui_rue.ts \
                translations/xdelta3-gui_ru.ts \
                translations/xdelta3-gui_rw.ts \
                translations/xdelta3-gui_sd.ts \
                translations/xdelta3-gui_si.ts \
                translations/xdelta3-gui_sk.ts \
                translations/xdelta3-gui_sl.ts \
                translations/xdelta3-gui_sm.ts \
                translations/xdelta3-gui_sn.ts \
                translations/xdelta3-gui_so.ts \
                translations/xdelta3-gui_sq.ts \
                translations/xdelta3-gui_sr.ts \
                translations/xdelta3-gui_st.ts \
                translations/xdelta3-gui_su.ts \
                translations/xdelta3-gui_sv.ts \
                translations/xdelta3-gui_sw.ts \
                translations/xdelta3-gui_ta.ts \
                translations/xdelta3-gui_te.ts \
                translations/xdelta3-gui_tg.ts \
                translations/xdelta3-gui_th.ts \
                translations/xdelta3-gui_tk.ts \
                translations/xdelta3-gui_tr.ts \
                translations/xdelta3-gui_tt.ts \
                translations/xdelta3-gui_ug.ts \
                translations/xdelta3-gui_uk.ts \
                translations/xdelta3-gui_ur.ts \
                translations/xdelta3-gui_uz.ts \
                translations/xdelta3-gui_vi.ts \
                translations/xdelta3-gui_xh.ts \
                translations/xdelta3-gui_yi.ts \
                translations/xdelta3-gui_yo.ts \
                translations/xdelta3-gui_yue_CN.ts \
                translations/xdelta3-gui_zh_CN.ts \
                translations/xdelta3-gui_zh_HK.ts \
                translations/xdelta3-gui_zh_TW.ts


