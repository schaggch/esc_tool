// Offline integration test: schema, firmware wire format and SPEEDPED page.
#include <QApplication>
#include <QSettings>
#include <QTabWidget>
#include <QFile>
#include <QFontDatabase>
#include <QXmlStreamReader>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickWidget>
#include <QQuickItem>
#include <QElapsedTimer>
#include <QThread>
#include "utility.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "configparams.h"
#include "pages/pagecustomconfig.h"
#include "bike_schema.h"
#undef assert
#define assert(x) do { if (!(x)) { std::fprintf(stderr,"CHECK FAILED line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    puts("Creating offline Qt application");
    qputenv("QT_QUICK_CONTROLS_CONF", ":/qtquickcontrols2_dark.conf");
    QApplication app(argc,argv);
    assert(QFontDatabase::addApplicationFont(":/res/fonts/DejaVuSans.ttf")>=0);
    app.setFont(QFont("DejaVu Sans",10));
    QCoreApplication::setOrganizationName("SPEEDPED-Tests");
    QCoreApplication::setApplicationName("SchemaTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QCoreApplication::applicationDirPath()+"/test-settings");
    ConfigParams p;
    puts("Checking schema and wire format");
    assert(p.loadParamsXml(":/res/config/bike.xml"));
    assert(p.getSignature()==BIKE_SIGNATURE);
    auto xml=qUncompress(bike_xml,sizeof(bike_xml));
    QXmlStreamReader reader(xml); ConfigParams fw;
    assert(fw.setParamsXML(reader)); assert(fw.getSignature()==p.getSignature());
    VByteArray bytes; p.serialize(bytes);
    assert(bytes.size()==BIKE_WIRE_SIZE);
    assert(bytes.toHex()=="2ea07d4c000008ca000000010000000000000000000001f400000005000000020100000064000001f4000027100000000f00000000000000000000006400000064");
    assert(!xml.contains("sh_erpm"));
    p.updateParamInt("pedal_kmh",25); p.updateParamInt("ah_kmh",6);
    p.updateParamEnum("pas_duty",0);
    p.updateParamInt("no_wheel_seconds",7); p.updateParamInt("no_wheel_watts",250);
    p.updateParamInt("no_wheel_erpm",1200);
    p.updateParamInt("sh_duty",10);
    p.updateParamInt("sound_start",1); p.updateParamInt("sound_volume",20); p.updateParamInt("sound_tempo",200);
    bytes.clear(); p.serialize(bytes); assert(fw.deSerialize(bytes));
    assert(fw.getParamInt("pedal_kmh")==25 && fw.getParamInt("ah_kmh")==6);
    assert(fw.getParamEnum("pas_duty")==0);
    assert(fw.getParamInt("no_wheel_seconds")==7 && fw.getParamInt("no_wheel_watts")==250);
    assert(fw.getParamInt("no_wheel_erpm")==1200);
    assert(fw.getParamInt("sh_duty")==10);
    assert(fw.getParamInt("sound_start")==1 && fw.getParamInt("sound_volume")==20 && fw.getParamInt("sound_tempo")==200);
    VescInterface vesc;
    puts("Creating SPEEDPED page");
    PageCustomConfig page; page.setVesc(&vesc); page.setBikeMode();
    auto tabs=page.findChild<QTabWidget*>(); assert(tabs && tabs->count()==3);
    assert(tabs->tabText(0)=="Speed" && tabs->tabText(1)=="PAS" && tabs->tabText(2)=="Sound");
    assert(!tabs->isEnabled()); // No controller: no writes to unrelated firmware.
    page.resize(1000,600); page.show(); app.processEvents();
    if(argc>1) assert(page.grab().save(QString::fromLocal8Bit(argv[1])));
    for(int mask=0;mask<16;mask++) {
        assert(QMetaObject::invokeMethod(&page,"updateBikePins",Qt::DirectConnection,Q_ARG(int,mask)));
        for(int pin=0;pin<4;pin++) {
            auto dot=page.findChild<QLabel*>(QString("speedpedPin%1").arg(pin)); assert(dot);
            assert(dot->styleSheet().contains(mask&(1<<pin) ? "#e53935" : "#27ae60"));
        }
    }
    QMetaObject::invokeMethod(&page,"updateBikePins",Qt::DirectConnection,Q_ARG(int,5));
    if(argc>1) assert(page.grab().save(QString::fromLocal8Bit(argv[1])+"-pins.png"));
    // Disconnected data must never make the display appear live.
    QMetaObject::invokeMethod(&page,"updateBikePins",Qt::DirectConnection,Q_ARG(int,-1));
    vesc.commands()->customHwDataReceived(QByteArray("SPEDPIN1",8)+char(15));
    for(int pin=0;pin<4;pin++)
        assert(page.findChild<QLabel*>(QString("speedpedPin%1").arg(pin))->styleSheet().contains("#808080"));
    tabs->setCurrentIndex(1); app.processEvents();
    if(argc>1) assert(page.grab().save(QString::fromLocal8Bit(argv[1])+"-pas.png"));
    tabs->setCurrentIndex(2); app.processEvents();
    if(argc>1) assert(page.grab().save(QString::fromLocal8Bit(argv[1])+"-sound.png"));
    puts("SPEEDPED Qt integration: schema signature, 65-byte wire format, round trip and page passed.");
    // Validate the same QML custom-config page used by the original mobile UI.
    assert(p.loadParamsXml(":/res/config/bike.xml"));
    qmlRegisterType<Commands>("Vedder.vesc.commands",1,0,"Commands");
    qmlRegisterType<ConfigParams>("Vedder.vesc.configparams",1,0,"ConfigParams");
    qmlRegisterType<VescInterface>("Vedder.vesc.vescinterface",1,0,"VescIf2");
    qmlRegisterType<Utility>("Vedder.vesc.utility",1,0,"Utility2");
    QQuickWidget view;
    view.setClearColor(Utility::getAppQColor("darkBackground"));
    Utility utility;
    Commands commands;
    auto context=view.rootContext();
    context->setContextProperty("Utility",&utility);
    context->setContextProperty("testConfig",&p);
    context->setContextProperty("testCommands",&commands);
    for(auto name : {"notchBot","notchLeft","notchRight"}) context->setContextProperty(name,0);
    QQmlComponent mock(view.engine());
    mock.setData(R"(import QtQuick 2.15
QtObject {
 property bool connected: true
 signal portConnectedChanged()
 signal customConfigLoadDone()
 function isPortConnected() { return connected }
 function customConfig(i) { return testConfig }
 function mcConfig() { return testConfig }
 function appConfig() { return testConfig }
 function commands() { return testCommands }
})",QUrl());
    QObject *mockVesc=mock.create(); assert(mockVesc);
    context->setContextProperty("VescIf",mockVesc);
    view.setResizeMode(QQuickWidget::SizeRootObjectToView);
    view.resize(390,844);
    view.setSource(QUrl("qrc:/mobile/ConfigPageCustom.qml"));
    for(const auto &error:view.errors()) qWarning()<<error;
    assert(view.status()==QQuickWidget::Ready && view.rootObject());
    view.show();
    assert(QMetaObject::invokeMethod(view.rootObject(),"reloadConfig"));
    auto settle=[&]() { QElapsedTimer timer; timer.start(); while(timer.elapsed()<150) {app.processEvents(); QThread::msleep(5);} };
    settle();
    auto section=view.rootObject()->findChild<QObject*>("customConfigSection"); assert(section);
    auto scroll=view.rootObject()->findChild<QObject*>("customConfigScroll"); assert(scroll);
    for(int i=0;i<3;i++) {
        section->setProperty("currentIndex",i); settle();
        int count=0;
        for(auto item:view.rootObject()->findChildren<QQuickItem*>()) {
            if(!item->property("paramName").isValid()) continue;
            assert(item->property("paramName").toString()!="sh_erpm");
            assert(!item->property("paramName").toString().startsWith("no_wheel_"));
            assert(item->width()>100 && item->width()<=390);
            count++;
        }
        assert(count==(i==0?5:4));
        if(i==0) assert(scroll->property("contentHeight").toDouble()>0);
        if(argc>1) assert(view.grab().save(QString::fromLocal8Bit(argv[1])+QString("-mobile-%1.png").arg(i)));
    }
    int maskReceived=-1;
    QObject::connect(&commands,&Commands::speedpedPinLevelsReceived,[&](int mask){maskReceived=mask;});
    QByteArray packet(1,char(COMM_CUSTOM_HW_DATA)); packet.append("SPEDPIN1",8); packet.append(char(5));
    commands.processPacket(packet); assert(maskReceived==5);
    packet[9]=char(0x80); commands.processPacket(packet); assert(maskReceived==5);
    view.resize(844,390); settle();
    if(argc>1) assert(view.grab().save(QString::fromLocal8Bit(argv[1])+"-mobile-landscape.png"));
    mockVesc->setProperty("connected",false);
    QMetaObject::invokeMethod(mockVesc,"portConnectedChanged"); settle();
    assert(!view.rootObject()->property("connected").toBool());
    puts("Mobile QML: portrait/landscape, all tabs, field removal, pin parsing and disconnect passed.");
    // This fixture tests configuration/UI only; skip device-service teardown.
    std::_Exit(0);
}
