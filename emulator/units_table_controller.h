#ifndef UNITS_TABLE_CONTROLLER_H
#define UNITS_TABLE_CONTROLLER_H

#include <QObject>

class Units_Table_Model;

class Units_Table_Controller : public QObject
{
    Q_OBJECT

private:
    Units_Table_Model *model_;

public:
    explicit Units_Table_Controller(Units_Table_Model *model, QObject *parent = nullptr);

signals:

public slots:
};

#endif // UNITS_TABLE_CONTROLLER_H
