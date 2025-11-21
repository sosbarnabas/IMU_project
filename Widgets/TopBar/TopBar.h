#pragma once
#include <QWidget>

class QButtonGroup;

class TopBar : public QWidget {
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent=nullptr);
    void setCurrentIndex(int index);
    int currentIndex() const;
    
signals:
    void tabChanged(int index);
    
protected:
    void paintEvent(QPaintEvent* e) override;
    
private:
    QButtonGroup* group_;
    void addTab(const QString& text, const QString& svgPath);
};
