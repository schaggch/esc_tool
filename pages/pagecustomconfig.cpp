/*
    Copyright 2020 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "pagecustomconfig.h"
#include "ui_pagecustomconfig.h"
#include "widgets/paramtable.h"
#include "utility.h"
#include <QFileDialog>
#include <QLabel>
#include <QTimer>

PageCustomConfig::PageCustomConfig(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageCustomConfig)
{
    ui->setupUi(this);
    mVesc = nullptr;
    mConfNum = 0;

    ui->readButton->setIcon(Utility::getIcon("/icons/Upload-96.png"));
    ui->readDefaultButton->setIcon(Utility::getIcon("/icons/Upload-96.png"));
    ui->writeButton->setIcon(Utility::getIcon("/icons/Download-96.png"));
    ui->saveXmlButton->setIcon(Utility::getIcon("/icons/Save as-96.png"));
    ui->loadXmlButton->setIcon(Utility::getIcon("/icons/Open Folder-96.png"));
}

PageCustomConfig::~PageCustomConfig()
{
    delete ui;
}

VescInterface *PageCustomConfig::vesc() const
{
    return mVesc;
}

void PageCustomConfig::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        connect(mVesc, SIGNAL(customConfigLoadDone()),
                this, SLOT(customConfigLoadDone()));
        connect(mVesc, &VescInterface::portConnectedChanged, this, [this]() {
            if (mBikeMode) { updateBikePins(-1); mPinAge.invalidate(); customConfigLoadDone(); }
        });
        connect(mVesc->commands(), &Commands::customHwDataReceived, this, [this](QByteArray data) {
            if (!mBikeMode || !isVisible() || !activeConfig() || data.size()!=9 ||
                    !data.startsWith("SPEDPIN1") || (quint8(data[8]) & 0xf0)) return;
            mPinAge.restart();
            updateBikePins(quint8(data[8]));
        });
    }
}

void PageCustomConfig::setConfNum(int num)
{
    mConfNum = num;
}

void PageCustomConfig::setBikeMode()
{
    if (mBikeMode) return;
    mBikeMode = true;
    mBikePreview = new ConfigParams(this);
    mBikePreview->loadParamsXml(":/res/config/bike.xml");
    auto note = new QLabel(tr("SPEEDPED75100 • ADC + UART • Sensorlos\n"
        "Hall A: SH · Hall B: AH (nur mit SH) · Hall C: Radsignal · alle aktiv bei GND.\n"
        "Gasgriff bestimmt die Leistung. SH loslassen nach 500 ms ohne AH: PAS 1 s gesperrt, dann neuer Safe-Start.\n"
        "Speedlimit 0 = unbegrenzt; SH-ADC 0 sperrt SH. Keine Ersatzbegrenzung bei fehlendem Radsignal. Mit Write speichern."), this);
    note->setWordWrap(true);
    ui->verticalLayout->insertWidget(0, note);
    auto pins = new QHBoxLayout;
    const QStringList names = {tr("SH (Hall A)"), tr("AH (Hall B)"), tr("Speedsensor (Hall C)"), tr("PAS (PPM)")};
    for (int i=0; i<4; ++i) {
        mPinDots[i] = new QLabel(this);
        mPinDots[i]->setObjectName(QString("speedpedPin%1").arg(i));
        mPinDots[i]->setFixedSize(16,16);
        mPinDots[i]->setAccessibleName(names[i]);
        pins->addWidget(mPinDots[i]);
        pins->addWidget(new QLabel(names[i],this));
        pins->addSpacing(12);
    }
    pins->addStretch();
    ui->verticalLayout->insertLayout(1,pins);
    auto legend = new QLabel(tr("Rot = GND / Low · Grün = High · Grau = keine aktuellen Daten (Abfrage alle 100 ms)"),this);
    legend->setWordWrap(true);
    legend->setToolTip(tr("Elektrische Pinpegel, unabhängig von Freigabe und Entprellung. Kurze Impulse können zwischen zwei Abfragen liegen."));
    ui->verticalLayout->insertWidget(2,legend);
    updateBikePins(-1);
    auto timer = new QTimer(this);
    connect(timer,&QTimer::timeout,this,[this]() {
        if (!isVisible() || !activeConfig()) { mPinAge.invalidate(); updateBikePins(-1); return; }
        if (!mPinAge.isValid() || mPinAge.elapsed()>500) updateBikePins(-1);
        mVesc->commands()->sendCustomHwData(QByteArray("SPEDPIN1",8));
    });
    timer->start(100);
    customConfigLoadDone();
}

void PageCustomConfig::updateBikePins(int mask)
{
    for(int i=0;i<4;i++) {
        if (!mPinDots[i]) continue;
        const bool low=mask>=0 && (mask & (1<<i));
        const QString color=mask<0 ? "#808080" : low ? "#e53935" : "#27ae60";
        const QString state=mask<0 ? tr("Keine aktuellen Daten") : low ? tr("GND / Low") : tr("High");
        mPinDots[i]->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 8px;").arg(color));
        mPinDots[i]->setToolTip(state);
        mPinDots[i]->setAccessibleDescription(state);
    }
}

ConfigParams *PageCustomConfig::activeConfig() const
{
    ConfigParams *p = mVesc ? mVesc->customConfig(mConfNum) : nullptr;
    if (mBikeMode && (!mVesc || !mVesc->isPortConnected() || !p ||
        p->getSignature() != mBikePreview->getSignature())) return nullptr;
    return p;
}

void PageCustomConfig::customConfigLoadDone()
{
    ConfigParams *p = activeConfig();
    if (mBikeMode) {
        for (auto button : {ui->readButton, ui->readDefaultButton, ui->writeButton,
                            ui->saveXmlButton, ui->loadXmlButton}) button->setEnabled(p != nullptr);
        ui->tabWidget->setEnabled(p != nullptr);
        ui->tabWidget->setToolTip(p ? QString() : tr("SPEEDPED75100 mit passender Firmware verbinden."));
        if (!p) p = mBikePreview;
    }
    if (p) {
        QStringList tabNames = p->getParamSubgroups("General");
        while (ui->tabWidget->count()) {
            delete ui->tabWidget->widget(0);
        }

        while (ui->tabWidget->count() < tabNames.size()) {
            ui->tabWidget->addTab(new ParamTable(ui->tabWidget), "");
        }

        for (int i = 0;i < tabNames.size();i++) {
            ui->tabWidget->setTabText(i, tabNames.at(i));
            ParamTable *t = dynamic_cast<ParamTable*>(ui->tabWidget->widget(i));
            if (mBikeMode) t->setWordWrap(false);
            t->clearParams();
            t->addParamSubgroup(p, "General", tabNames.at(i));
            if (mBikeMode) {
                int width=0;
                for(int row=0;row<t->rowCount();row++) {
                    if(t->item(row,0)) width=qMax(width,t->fontMetrics().horizontalAdvance(t->item(row,0)->text())+24);
                }
                t->setColumnWidth(0,width);
            }
        }
    }
}

void PageCustomConfig::on_readButton_clicked()
{
    if (mVesc && activeConfig()) {
        mVesc->commands()->customConfigGet(mConfNum, false);
    }
}

void PageCustomConfig::on_readDefaultButton_clicked()
{
    if (mVesc && activeConfig()) {
        mVesc->commands()->customConfigGet(mConfNum, true);
    }
}

void PageCustomConfig::on_writeButton_clicked()
{
    if (mVesc) {
        ConfigParams *params = activeConfig();
        if (params) {
            mVesc->commands()->customConfigSet(mConfNum, params);
        }
    }
}

void PageCustomConfig::on_saveXmlButton_clicked()
{
    if (!activeConfig()) return;
    QString path;
    path = QFileDialog::getSaveFileName(this,
                                        tr("Choose where to save the configuration XML file"),
                                        ".",
                                        tr("Xml files (*.xml)"));

    if (path.isNull()) {
        return;
    }

    if (!path.toLower().endsWith(".xml")) {
        path += ".xml";
    }

    if (!activeConfig()) return;
    bool res = activeConfig()->saveXml(path, "CustomConfiguration");

    if (res) {
        mVesc->emitStatusMessage("Saved custom configuration", true);
    } else {
        mVesc->emitMessageDialog(tr("Save custom configuration"),
                                 tr("Could not save custom configuration:<BR>"
                                    "%1").arg(mVesc->mcConfig()->xmlStatus()),
                                 false, false);
    }
}

void PageCustomConfig::on_loadXmlButton_clicked()
{
    if (!activeConfig()) return;
    QString path;
    path = QFileDialog::getOpenFileName(this,
                                        tr("Choose custom configuration file to load"),
                                        ".",
                                        tr("Xml files (*.xml)"));

    if (path.isNull()) {
        return;
    }

    if (!activeConfig()) return;
    bool res = activeConfig()->loadXml(path, "CustomConfiguration");

    if (res) {
        mVesc->emitStatusMessage("Loaded custom configuration", true);
    } else {
        mVesc->emitMessageDialog(tr("Load custom configuration"),
                                 tr("Could not load custom configuration:<BR>"
                                    "%1").arg(mVesc->mcConfig()->xmlStatus()),
                                 false, false);
    }
}
