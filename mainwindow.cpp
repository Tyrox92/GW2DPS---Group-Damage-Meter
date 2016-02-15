#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_configurator.h"
#include <QUrl>
#include <QDesktopServices>
#include <QtDebug>
#include "screenrecorder.h"
#include "settings.h"
#include "global.h"
#include <QSizeGrip>
#include <QToolBar>
#include <QToolButton>
#include "ui_mydialog.h"
#include <QtNetwork>

using namespace GW2;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_Configurator(this),
    m_MyDialog(this),
    update_Timer(this)
{
    ui->setupUi(this);
    StartupPref();

    QObject::connect(&update_Timer, SIGNAL(timeout()), this, SLOT(UpdateTimer()));

    ScreenRecorder* screenRecorder = new ScreenRecorder;
    DmgMeter* dmgMeter = &screenRecorder->GetDmgMeter();
    screenRecorder->moveToThread(&m_ScreenRecorderThread);

    Ui::Configurator* uiConfig = m_Configurator.ui;

    dmgMeter->moveToThread(&m_ScreenRecorderThread);

    QObject::connect(&m_ScreenRecorderThread, SIGNAL(finished()), screenRecorder, SLOT(deleteLater()));
    QObject::connect(&m_ScreenRecorderThread, SIGNAL(started()), screenRecorder, SLOT(StartRecording()));
    QObject::connect(screenRecorder, SIGNAL(RequestInfoUpdate(QString)), ui->labelInfo, SLOT(setText(QString)));
    QObject::connect(dmgMeter, SIGNAL(RequestTimeUpdate(int)), this, SLOT(UpdateTime(int)));
    QObject::connect(ui->actionReset, SIGNAL(triggered()), dmgMeter, SLOT(Reset()));
    QObject::connect(ui->actionAutoReset, SIGNAL(triggered(bool)), dmgMeter, SLOT(SetIsAutoResetting(bool)));
    QObject::connect(ui->actionEnableTransparency, SIGNAL(triggered(bool)), this, SLOT(EnableTransparency(bool)));
    QObject::connect(ui->actionHelp, SIGNAL(triggered()), this, SLOT(LinkToWebsite()));
    QObject::connect(ui->actionConfig, SIGNAL(triggered()), &m_Configurator, SLOT(exec()));
    QObject::connect(uiConfig->comboBoxScreenshots, SIGNAL(currentIndexChanged(QString)), screenRecorder, SLOT(SetScreenshotsPerSecond(QString)));
    QObject::connect(uiConfig->comboBoxUpdates, SIGNAL(currentIndexChanged(QString)), dmgMeter, SLOT(SetUpdatesPerSecond(QString)));
    QObject::connect(uiConfig->comboBoxSecondsInCombat, SIGNAL(currentIndexChanged(QString)), dmgMeter, SLOT(SetSecondsInCombat(QString)));
    QObject::connect(uiConfig->comboBoxConsideredLines, SIGNAL(currentIndexChanged(QString)), dmgMeter, SLOT(SetConsideredLineCount(QString)));
    QObject::connect(uiConfig->pushButtonReset, SIGNAL(clicked(bool)), &m_Configurator, SLOT(RestoreDefaults()));
    QObject::connect(uiConfig->checkBoxProfColors, SIGNAL(clicked(bool)), this, SLOT(ProfSettingsChanged()));
    QObject::connect(uiConfig->checkBoxDamageDone, SIGNAL(clicked(bool)), this, SLOT(DamageDoneChanged()));
    QObject::connect(uiConfig->checkBoxPosition, SIGNAL(clicked(bool)), this, SLOT(PositionChanged()));
    QObject::connect(uiConfig->professionComboBox, SIGNAL(currentIndexChanged(QString)), this, SLOT(ProfChanged(QString)));

    dmgMeter->SetUpdatesPerSecond(uiConfig->comboBoxUpdates->currentText());
    dmgMeter->SetSecondsInCombat(uiConfig->comboBoxSecondsInCombat->currentText());
    dmgMeter->SetConsideredLineCount(uiConfig->comboBoxConsideredLines->currentText());

    m_ScreenRecorderThread.start();

    //Settings::ReadSettings<QMainWindow>(this);

    // Start screenshot timer from separate thread
    const int oldIndex = uiConfig->comboBoxScreenshots->currentIndex();
    uiConfig->comboBoxScreenshots->setCurrentIndex((uiConfig->comboBoxScreenshots->currentIndex() + 1) % uiConfig->comboBoxScreenshots->count());
    uiConfig->comboBoxScreenshots->setCurrentIndex(oldIndex);

    // Profession Color Bars
    ProfBasedColors=uiConfig->checkBoxProfColors->isChecked();
    pPosition=uiConfig->checkBoxPosition->isChecked();
    pDamageDone=uiConfig->checkBoxDamageDone->isChecked();
    uiConfig->professionComboBox->setCurrentIndex(0);
    m_MyProfession=uiConfig->professionComboBox->currentIndex();

    // We are not connected on start up
    is_connected = false;

    CheckFirstRun();
    CheckForUpdate();
    Initialize();
}

void MainWindow::ProfChanged(QString prof)
{
    if (prof== "Elementalist") m_MyProfession=1;else
        if (prof== "Engineer") m_MyProfession=2;else
            if (prof== "Guardian") m_MyProfession=3;else
                if (prof== "Mesmer") m_MyProfession=4;else
                    if (prof== "Necromancer") m_MyProfession=5;else
                        if (prof== "Ranger") m_MyProfession=6;else
                            if (prof== "Revenant") m_MyProfession=7;else
                                if (prof== "Thief") m_MyProfession=8;else
                                    if (prof== "Warrior") m_MyProfession=9;
}

void MainWindow::CheckFirstRun()
{
    //First Run checks
    QString tmp1=Read1stRun();
    if (tmp1!="OK")
    {
        //show a new window with explanation of the correct gw2 settings

        QDialog *dialog1 = new QDialog();
        QHBoxLayout *layout = new QHBoxLayout(dialog1);
        QLabel *label1 = new QLabel(this);
        label1->setText("Welcome to GW2DPS!\n\nPlease set up the following options in your Guild Wars2:\n\n -Options/Graphics Options: Interface Size= Small/Normal\n -Options/Graphics Options: Resolution=Windowed Fullscreen\n -Chatbox/options: Text Size=Medium\n -Chatbox/options: Disable Timestamps\n -Chatbox/Combat page/options: enable only : Outgoing Buff Damage+Outgoing Damage+Outgoing Mitigated Damage\n -Make sure your combat log has more then 12+ lines and always visible\n\n Have fun!");
        layout->addWidget(label1);
        layout->setMargin(10);
        //dialog1->setStyleSheet("background:red;");
        dialog1->show();

        Write1stRun("OK");
    }
}

void MainWindow::ProfSettingsChanged()
{
    if (ProfBasedColors==1) ProfBasedColors=0; else ProfBasedColors=1;
}
void MainWindow::PositionChanged()
{
    if (pPosition==1) pPosition=0; else pPosition=1;
}
void MainWindow::DamageDoneChanged()
{
    if (pDamageDone==1) pDamageDone=0; else pDamageDone=1;
}

void MainWindow::UpdateGroupLabels()
{
    QLabel* Label1;
    QProgressBar* Bar1 = ui->progressBar_1;
    QProgressBar* Bar2 = ui->progressBar_2;
    QProgressBar* Bar3 = ui->progressBar_3;
    QProgressBar* Bar4 = ui->progressBar_4;
    QProgressBar* Bar5 = ui->progressBar_5;
    QProgressBar* Bar6 = ui->progressBar_6;
    QProgressBar* Bar7 = ui->progressBar_7;
    QProgressBar* Bar8 = ui->progressBar_8;
    QProgressBar* Bar9 = ui->progressBar_9;
    QProgressBar* Bar10 = ui->progressBar_10;
    QProgressBar* Bar [10] = {Bar1,Bar2,Bar3,Bar4,Bar5,Bar6,Bar7,Bar8,Bar9,Bar10};
    long p,i,j,k;

    // If playing without a server
    // Display only the solo user information
    if ((is_connected == false) && MyClientSlot == 10)
    {
        //StartupHideProgressBars();
        PosDmg[0]=m_Dmg;
        PosDPS[0]=m_Dps;
        PosAct[0]=m_Activity;
        if (PosDmg[0]>0 )i=PosDmg[0]*100.0/PosDmg[0];else i=0;
        //if (AllDamageDone>0)p=PosDmg[0]*100/AllDamageDone;else p=0;
        Bar[0]->setValue(i);
        QString text = QString("%L2 DMG - [%L3 DPS]").arg(PosDmg[0]).arg(PosDPS[0]);
        Bar[0]->setFormat(text);
        Bar[0]->setVisible(true);
        //AllDamageDone=m_Dmg;
        ui->grp_Dmg->setText(QString::number(m_Dmg));
    }
    else
    {

        AllDamageDone=0;
        for (j=0;j<10;j++) AllDamageDone+=SlotDmg[j];
        Label1 = ui->grp_Dmg;
        Label1->setText(QString::number(AllDamageDone));
        GrpDPS=0;
        for (j=0;j<10;j++) GrpDPS+=SlotDPS[j];
        Label1 = ui->grp_DPS;
        Label1->setText(QString::number(GrpDPS));
        i=0;
        for (j=0;j<10;j++) if (SlotDPS[j]>0) i++;
        if (i>0) AvgDPS=GrpDPS/i; else AvgDPS=0;
        Label1 = ui->avg_DPS;
        Label1->setText(QString::number(AvgDPS));

        for (j=0;j<10;j++)
        {
            PosDmg[j]=SlotDmg[j];
            PosDPS[j]=SlotDPS[j];
            PosAct[j]=SlotAct[j];
            PosProf[j]=SlotProf[j];
            strcpy(PosName[j],SlotName[j]);
        }

        k=0;
        for (i=0;i<9;i++)
        {
            for (j=i+1;j<10;j++)
            {
                if (PosDmg[j]>PosDmg[i])
                {
                    k=PosDmg[i];
                    PosDmg[i]=PosDmg[j];
                    PosDmg[j]=k;
                    k=PosDPS[i];
                    PosDPS[i]=PosDPS[j];
                    PosDPS[j]=k;
                    k=PosAct[i];
                    PosAct[i]=PosAct[j];
                    PosAct[j]=k;
                    k=PosProf[i];
                    PosProf[i]=PosProf[j];
                    PosProf[j]=k;
                    strcpy(tmp1,PosName[i]);
                    strcpy(PosName[i],PosName[j]);
                    strcpy(PosName[j],tmp1);
                }
            }
            for (int p=0;p<10;p++)
            {
                if (PosName[p]==QString("Disconnected"))
                {
                    PosName[p][0]=0;
                    PosDmg[p]=0;
                    PosDPS[p]=0;
                    PosAct[p]=0;
                    PosProf[p]=0;
                }
            }
        }

        for(int n=0;n<10;n++) {
            if (PosName[n][0]!=0) {
                if (PosDmg[0]>0)i=PosDmg[n]*100/PosDmg[0];
                else i=0;
                if (AllDamageDone>0)p=PosDmg[n]*100/AllDamageDone;
                else p=0;
                Bar[n]->setValue(i);
                //QString text = QString("%1. %2 %L3% [%L4 DPS]").arg(n+1).arg(PosName[n]).arg(p).arg(PosDPS[n]);
                QString text;
                if (pPosition>0)
                {
                    if (pDamageDone>0) text = QString("%1. %2 %L3% [%L4] [%L5 DPS]").arg(n+1).arg(PosName[n]).arg(p).arg(PosDmg[n]).arg(PosDPS[n]);
                    else text = QString("%1. %2 %L3% [%L4 DPS]").arg(n+1).arg(PosName[n]).arg(p).arg(PosDPS[n]);
                    Bar[n]->setFormat(text);
                }
                if (pPosition<1)
                {
                    if (pDamageDone<1) text = QString("%1 %L2% [%L3 DPS]").arg(PosName[n]).arg(p).arg(PosDPS[n]);
                    else text = QString("%1 %L2% [%L3] [%L4 DPS]").arg(PosName[n]).arg(p).arg(PosDmg[n]).arg(PosDPS[n]);
                    Bar[n]->setFormat(text);
                }

                Bar[n]->setAlignment(Qt::AlignRight);
                Bar[n]->setVisible(true);
                if (ProfBasedColors>0)
                {
                    //profession based bar colors
                    switch (PosProf[n])
                    {
                    case 0:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(3, 132, 146 , 60%);}");
                        break;
                    case 1:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(236, 87, 82, 70%);}");
                        break;
                    case 2:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(153,102,51, 70%);}");
                        break;
                    case 3:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(51,153,204, 70%);}");
                        break;
                    case 4:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(153,51,153, 70%);}");
                        break;
                    case 5:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(51,153,102, 70%);}");
                        break;
                    case 6:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(102,204,51, 70%);}");
                        break;
                    case 7:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(204,99,66, 70%);}");
                        break;
                    case 8:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(204,102,102, 70%);}");
                        break;
                    case 9:
                        Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(255,153,51, 70%);}");
                        break;
                    }
                }
                else
                {
                    if (n%2==0 ) Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(3, 132, 146 , 60%);}");
                    else Bar[n]->setStyleSheet("QProgressBar {border: 0px solid grey;border-radius:0px;font: 87 10pt DINPro-Black;color: rgb(255, 255, 255);text-align: center;min-height: 15px;margin: 0.5px;}QProgressBar::chunk {background-color: rgba(4,165,183, 60%);}");
                }



                // For Future Release Adding Labels to Align Text within the Progressbar properly



                //                //Disable normal Text
                //                Bar[n]->setTextVisible(false);

                //                QVBoxLayout(Bar[n]).addWidget(nameLabel);
                //                QVBoxLayout(Bar[n]).addWidget(dmgLabel);
                //                //Position and Name
                //                QString myname = QString("%1. %2").arg(n+1).arg(PosName[n]);
                //                // DPS and %
                //                QString myDmg = QString("%L3% [%L4 DPS]").arg(p).arg(PosDPS[n]);
                //                //Set Text
                //                nameLabel->setText(myname);
                //                dmgLabel->setText(myDmg);
                //                //Styling them
                //                nameLabel->setStyleSheet("color:white;background:none;margin-top:50%;font-size:12px;");
                //                dmgLabel->setStyleSheet("color:white;background:none;");
                //                //Align Them
                //                nameLabel->setMinimumHeight(17);
                //                dmgLabel->setMinimumSize(100,17);


                //                //Display Labels
                //                nameLabel->show();
                //                dmgLabel->show();




            }
            else
                Bar[n] ->setVisible(false);
        }
    }
}

void MainWindow::ready2Read()
{

    int i,j;
    long k;


    incData = socket->readAll();
    incDataSize = incData.size();
    memcpy(incData2, incData.data(), incDataSize);

    i=0;

    if (MyClientSlot==10)
    {
        if ((incDataSize==4) && (incData2[0]=='*') && (incData2[1]=='*') && (incData2[2]=='*'))
        {
            MyClientSlot= incData2[3]-48;

        }
    }
    else
    {
        while (i<incDataSize)
        {
            if ((incData2[i]=='*') && (incData2[i+3]=='#'))
            {

                if ((incData2[i+1]<58) && (incData2[i+1]>47) && (incData2[i+2]>48) && (incData2[i+2]<54))
                {
                    CurrentPos=incData2[i+1]-48;
                    CurrentMeta=incData2[i+2]-48;
                }
                i+=3;

            }
            else if (incData2[i]=='#')
            {
                if (CurrentMeta==1)
                {

                    j=i+1;
                    while ((j-i-1<13) && (j<incDataSize) && (incData2[j]!='*')) { SlotName[CurrentPos][j-i-1]=incData2[j];j++; }
                    if (incData2[j]=='*') SlotName[CurrentPos][j-i-1]=0; else SlotName[CurrentPos][0]=0;
                    i=j;

                }else
                {



                    j=i+1;k=0;
                    while ((j-i-1<12) && (j<incDataSize) && (incData2[j]!='*')&& (incData2[j]>47)&& (incData2[j]<58)) { k=k*10+incData2[j]-48;j++; }
                    if  (incData2[j]=='*')
                    {

                        if  (CurrentMeta==2) SlotDPS[CurrentPos]=k;
                        if  (CurrentMeta==3) SlotDmg[CurrentPos]=k;
                        if  (CurrentMeta==4) SlotAct[CurrentPos]=k;
                        if  (CurrentMeta==5) SlotProf[CurrentPos]=k;

                    }
                    i=j;
                }
            }
            else { i++; while((i<incDataSize) && (incData2[i])!='*') i++; }

        }

    }


}

void MainWindow::connected()
{
    qDebug() << "connected...";
    MyClientSlot=10;  //no handshake yet
}

void MainWindow::disconnected()
{
    is_connected = false;MyClientSlot=10;
    qDebug() << "disconnected...";
    //so what now? exit?
}

MainWindow::~MainWindow()
{
    m_ScreenRecorderThread.quit();
    Settings::WriteSettings<QMainWindow>(this);
    if (!m_ScreenRecorderThread.wait(1000))
    {
        // Failed to exit thread => terminate it.
        m_ScreenRecorderThread.terminate();
        m_ScreenRecorderThread.wait();
    }

    delete ui;
}

void MainWindow::EnableTransparency(bool isAlmostTransparent)
{
    if (isAlmostTransparent)
    {
        this->ui->centralWidget->setStyleSheet("background-color: rgba(32, 43, 47, 0%);");
        ui->toolBar->setStyleSheet("QWidget { background-color: rgba(32, 43, 47, 1%); } QToolButton { background-color: rgba(32, 43, 47, 1%); }");
        ui->grp_DPS->setStyleSheet("");
        this->show();
    }
    else
    {
        this->ui->centralWidget->setStyleSheet("background-color: rgba(32, 43, 47, 60%);");
        ui->toolBar->setStyleSheet("QWidget { background-color: rgba(32, 43, 47, 60%); } QToolButton { background-color: rgba(32, 43, 47, 1%); }");
        ui->grp_DPS->setStyleSheet("");
        this->show();
    }
}

void MainWindow::LinkToWebsite()
{
    QDesktopServices::openUrl(QUrl(MAINWINDOW_WEBSITE_URL));
}

void MainWindow::SendClientInfo(void)
{
    QByteArray tmp1;
    const char* tmp2;

    tmp1 = MyName.toLatin1();
    tmp2 = tmp1.data();
    if (MyClientSlot!=10)  //connected and semi-handshaked
    {
        //Failsafe Checks
        if (m_Dps>99999) m_Dps = 1;
        if (m_Dmg>999999999) m_Dmg = 1;
        if (m_Activity>100) m_Activity = 1;

        sprintf(writeBuff, "*%u1#%s*%u2#%lu*%u3#%lu*%u4#%lu*%u5#%lu*", MyClientSlot, tmp2 , MyClientSlot, m_Dps, MyClientSlot, m_Dmg, MyClientSlot, m_Activity,MyClientSlot, m_MyProfession);
        socket->write(writeBuff);
    }
}

void MainWindow::UpdatePersonalLabels()
{
    QLabel* Label1;
    unsigned long c;
    double c1,c2,c3,c4;

    if ((is_connected == true))
    {
        ui->grp_DPS->setStyleSheet("color: rgb(255, 255, 255);");
    }
    else
    {
        Label1 = ui->grp_DPS;
        if (m_Dps > DMGMETER_HIGH_DPS_LIMIT)
        {
            Label1->setStyleSheet(DmgMeter::s_UltraStyle);
        }
        else if (m_Dps > DMGMETER_NORMAL_DPS_LIMIT)
        {
            Label1->setStyleSheet(DmgMeter::s_HighStyle);
        }
        else if (m_Dps > DMGMETER_LOW_DPS_LIMIT)
        {
            Label1->setStyleSheet(DmgMeter::s_NormalStyle);
        }
        else
        {
            Label1->setStyleSheet(DmgMeter::s_LowStyle);
        }
    }
    //Personal Crit Chance Value
    Label1 = ui->critChance;
    Label1->setText(QString::number(m_critChance));
    //Personal Condi DMG Value
    Label1 =ui->labelCondiDMGValue;
    Label1->setText(QString::number(m_condiDmg));
    //Personal Condi DPS Value
    c2=m_condiDmg;
    c3=m_Dps;
    c4=m_Dmg;
    c1=c2*c3;
    if (m_Dmg>0)c=round(c1/c4);else c=0;
    Label1 = ui->condiDPS;
    Label1->setText(QString::number(c));
    //Personal Max Damage Value
    Label1 = ui->labelMaxDmgValue;
    Label1->setText(QString::number(m_MaxDmg));
    if (m_MaxDmg > DMGMETER_HIGH_DPS_LIMIT)
    {
        Label1->setStyleSheet(DmgMeter::s_UltraStyle);
    }
    else if (m_MaxDmg > DMGMETER_NORMAL_DPS_LIMIT)
    {
        Label1->setStyleSheet(DmgMeter::s_HighStyle);
    }
    else if (m_MaxDmg > DMGMETER_LOW_DPS_LIMIT)
    {
        Label1->setStyleSheet(DmgMeter::s_NormalStyle);
    }
    else
    {
        Label1->setStyleSheet(DmgMeter::s_LowStyle);
    }
}

void MainWindow::UpdateTimer(void)
{
    if ((is_connected == true))
    {
        ui->actionConnect->setIcon(QIcon(":/connected"));
        SendClientInfo();
    }
    else ui->actionConnect->setIcon(QIcon(":/connect"));
    UpdateGroupLabels();
    UpdatePersonalLabels();
}

void MainWindow::UpdateTime(int timeInMsecs)
{
    int secs = timeInMsecs / 1000;
    int mins = secs / 60;
    secs -= mins * 60;
    int hours = mins / 60;
    mins -= hours * 60;
    QString time;
    if (hours > 0)
    {
        time += QString::number(hours) + "h ";
    }

    if (mins > 0)
    {
        time += QString::number(mins) + "m ";
    }

    time += QString::number(secs) + "s";
    ui->labelTimeValue->setText(time);
}

void MainWindow::Initialize()
{
    if (HostIP != "" && (is_connected == false))
    {
        socket = new QTcpSocket(this);
        connect(socket, SIGNAL(connected()),this, SLOT(connected()));
        connect(socket, SIGNAL(disconnected()),this, SLOT(disconnected()));
        connect(socket, SIGNAL(readyRead()),this, SLOT(ready2Read()));
        qDebug() << "connecting to : " << HostIP << ":" << HostPort;

        MyClientSlot=10; //no semi-handshake yet
        CurrentMeta=0;CurrentPos=0;
        int i;
        for (i=0;i<10;i++)
        {
            SlotDmg[i]=0;
            SlotDPS[i]=0;
            SlotAct[i]=0;
            SlotName[i][0]='\0';
        }
        AllDamageDone=0;
        hitCounter=0;
        m_critChance=0;
        critCounter=0;
        m_condiDmg=0;
        LastColor=0;

        socket->connectToHost(HostIP, HostPort);

        if(!socket->waitForConnected(5000))
        {
            qDebug() << "Error: " << socket->errorString();
            QDialog *dialog = new QDialog();
            QHBoxLayout *layout = new QHBoxLayout(dialog);
            QLabel *label = new QLabel(this);
            label->setText("Connection to " + HostIP + " failed");
            layout->addWidget(label);
            layout->setMargin(50);
            dialog->setStyleSheet("background:red;");
            dialog->show();
            is_connected = false;
        }
        else is_connected = true;
        //m_MyProfession=0;
        // we need to wait...
        m_Dps=0;m_Dmg=0;m_Activity=0;m_MaxDmg=0;
        update_Timer.start(1000);
    }
    else
    {
        is_connected = false;

        // this is not blocking call
        MyClientSlot=10; //no semi-handshake yet
        CurrentMeta=0;CurrentPos=0;

        int i;
        for (i=0;i<10;i++)
        {
            SlotDmg[i]=0;
            SlotDPS[i]=0;
            SlotAct[i]=0;
            SlotName[i][0]='\0';
        }
        AllDamageDone=0;
        hitCounter=0;
        m_critChance=0;
        critCounter=0;
        m_condiDmg=0;
        LastColor=0;
        //m_MyProfession=0;
        // we need to wait...
        m_Dps=0;m_Dmg=0;m_Activity=0;m_MaxDmg=0;
        update_Timer.start(1000);
    }
}

// Give movement access to MainWindow
void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

// Give movement access to MainWindow
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

//Shrink UI to ToolBar
void GW2::MainWindow::on_actionShrinkUI_triggered(bool checked)
{
    if (checked)
    {
        ui->centralWidget->hide();
    }
    else
    {
        ui->centralWidget->show();
    }
}

//Show Condi DPS/Crit%/Condi DMG/Highest Hit if ". . ." icon is actionActionGroupDetails is toggled on/off.
bool GW2::MainWindow::on_pushButton_toggled(bool toggeled)
{
    if (toggeled)
    {
        ui->widgetExtraDetails->show();
        toggeled = true;
    }
    else
    {
        ui->widgetExtraDetails->hide();
        toggeled = false;
    }
    return toggeled;
}

void GW2::MainWindow::StartupPref()
{
    ui->toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    this->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    //ui->toolBar->setWindowFlags(Qt::WindowStaysOnTopHint);
    //ui->toolBar->setAttribute(Qt::WA_TranslucentBackground);
    ui->widget->hide();
    ui->widgetExtraDetails->hide();

    // Resize Option
    // Using gridLayout here which is the main layout
    QSizeGrip *sizeGripRight = new QSizeGrip(this);
    QSizeGrip *sizeGripLeft = new QSizeGrip(this);
    ui->gridLayout_2->addWidget(sizeGripRight, 0,0,10,10,Qt::AlignBottom | Qt::AlignRight);
    ui->gridLayout_2->addWidget(sizeGripLeft, 0,0,10,10,Qt::AlignBottom | Qt::AlignLeft);
    sizeGripLeft->setStyleSheet("background: url(''); width: 20px; height: 20px;");
    sizeGripRight->setStyleSheet("background: url(''); width: 20px; height: 20px;");

    ui->progressBar_1->setVisible(false);
    ui->progressBar_2->setVisible(false);
    ui->progressBar_3->setVisible(false);
    ui->progressBar_4->setVisible(false);
    ui->progressBar_5->setVisible(false);
    ui->progressBar_6->setVisible(false);
    ui->progressBar_7->setVisible(false);
    ui->progressBar_8->setVisible(false);
    ui->progressBar_9->setVisible(false);
    ui->progressBar_10->setVisible(false);
}

//Show player/group details
bool GW2::MainWindow::on_actionActionGroupDetails_toggled(bool toggeled)
{
    // Check if this user is playing on a server or not
    if ((is_connected == true))
    {
        if (toggeled)
        {
            ui->avg_DPS->show();
            ui->grp_DPS->show();
            ui->grp_Dmg->show();
            ui->labelDmg_2->show();
            ui->labelDmg_3->show();
            ui->labelDmg_3->setText("Grp DPS");
            ui->labelDmg_4->show();
            MainWindow::ui->widget->show();
            toggeled = true;
        }
        else
        {
            ui->widget->hide();
            toggeled = false;
        }
    }
    // If playing solo - show only DPS/DMG/TIME
    else
    {
        if (toggeled)
        {
            ui->avg_DPS->hide();
            ui->grp_DPS->hide();
            ui->labelDmg_3->hide();
            ui->labelDmg_4->hide();
            ui->grp_Dmg->hide();
            ui->labelDmg_2->hide();
            MainWindow::ui->widget->show();
            toggeled = true;
        }
        else
        {
            ui->widget->hide();
            toggeled = false;
        }
    }
    return toggeled;
}

void GW2::MainWindow::on_actionConnect_triggered()
{
    Ui::Configurator* uiConfig = m_Configurator.ui;

    if (ui->actionActionGroupDetails->isChecked())
        ui->actionActionGroupDetails->setChecked(false);
    if (ui->pushButton->isChecked())
        ui->pushButton->setChecked(false);

    // If not connected to a server when triggered
    // Then open myDialog
    if ((is_connected == false))
    {
        update_Timer.stop();
        MyDialog mDialog;
        mDialog.setModal(true);


        mDialog.exec();

        MyName=mDialog.getName();
        HostIP = mDialog.getIP();
        HostPort=mDialog.getPort().toInt();
        m_MyProfession=mDialog.getProfession();
        uiConfig->professionComboBox->setCurrentIndex(m_MyProfession);
        Initialize();
    }
    // Otherwise stop the timer and abort the connection
    else
    {
        update_Timer.stop();
        socket->abort();
        StartupPref();
        is_connected = false;
        HostIP="";

        //Go back to the initializer
        Initialize();
    }
}

void GW2::MainWindow::on_actionClose_triggered()
{
    // Let's abort the connection to the server before closing
    if ((is_connected == true)) socket->abort();
}

void GW2::MainWindow::CheckForUpdate()
{
    QString curVersion = Settings::s_Version;

    QNetworkAccessManager *nam = new QNetworkAccessManager();
    QUrl data_url("http://www.gw2dps.com/version/version_check.txt");
    QNetworkReply* reply = nam->get(QNetworkRequest(data_url));
    QEventLoop eventLoop;
    QObject::connect(reply, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();
    if (reply->error() != QNetworkReply::NoError)
    {
        // Something went wrong. Error can be retrieved with: reply->error()
        qDebug() << reply->error();
    }
    else
    {
        // Call reply->readAll() and do what you need with the data
        QString ver = reply->readAll();
        qDebug() << ver;
        if(curVersion < ver)
        {
            qDebug() << "You need to Update";
            QDialog *dialog = new QDialog();
            QHBoxLayout *layout = new QHBoxLayout(dialog);
            QLabel *label = new QLabel();
            label->setText("A newer Version of GW2DPS is available!<br><hr><br><a href='http://gw2dps.com/download'>Download new Version</a>");
            label->setTextFormat(Qt::RichText);
            label->setTextInteractionFlags(Qt::TextBrowserInteraction);
            label->setOpenExternalLinks(true);
            layout->addWidget(label);
            layout->setMargin(50);
            dialog->setStyleSheet("background:#f2f2f2;");
            dialog->setWindowFlags(Qt::WindowStaysOnTopHint);
            dialog->show();
        }
    }
}
