#ifndef SAVELOG_H
#define SAVELOG_H

#include <QDialog>

namespace Ui {
class saveLog;
}

namespace GW2
{
    class saveLog : public QDialog
    {
        Q_OBJECT

    public:
        explicit saveLog(QWidget *parent = 0);
        ~saveLog();

    private:
        Ui::saveLog *ui;

        friend class MainWindow;
    };
}

#endif // SAVELOG_H
