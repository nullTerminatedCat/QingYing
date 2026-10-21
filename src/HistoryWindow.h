#pragma once
#include <QDialog>

#include <QList>

class QListWidget;
class QLabel;
class QPushButton;

class HistoryWindow : public QDialog {
    Q_OBJECT
public:
    explicit HistoryWindow(QWidget* parent = nullptr);
    void reload();

private:
    QString currentPath() const;
    QListWidget* m_list;
    QLabel* m_empty = nullptr;
    QList<QPushButton*> m_needItem;
};
