#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE
/*Qt 的宏封装，用于处理 Qt Creator 自动生成的 UI 命名空间（名字空间通常是 Ui::Widget），防止命名冲突*/

class Widget : public QWidget
{
    Q_OBJECT
    /*启用信号和槽机制*/

public:
    Widget(QWidget *parent = nullptr);//初始化窗口
    ~Widget();

private:
    Ui::Widget *ui;//指向UI空间集的指针，用于访问界面控制，例如ui->pushButton->setText(...)
};
#endif // WIDGET_H
