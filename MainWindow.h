#ifndef MAINWINDOW_H
#define MAINWINDOW_H
/*MainWindow演程序主窗口的角色。它负责创建和管理菜单栏 ，将用户的菜单选择传递给核心的绘图控件*/
/*MainWindow只关心窗口框架和菜单*/
/*MainWindow 就像一个总控制器，把菜单命令发给 DrawingWidget，由它执行底层逻辑*/
/*MainWindow 是程序的“窗口控制层”，只负责 UI 框架和菜单传达；具体的几何任务由 DrawingWidget 执行，它们通过信号槽连接紧密协作*/
#include <QMainWindow>
#include "Function.h"

//前向声明，因为我们只用指针
class QStatusBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    //个槽函数用来接收DrawingWidget的信号并更新状态栏
    void updateStatus(const QString& message);
    void onPolygonsReady(bool ready);

private:
    void createMenus();
    void closeEvent(QCloseEvent *event) override;

    DrawingWidget *drawingWidget;
    QStatusBar *mainStatusBar; //状态栏指针

    QMenu *intersectionUnionMenu; // “交集/并集”主菜单
    QMenu *intersectionMenu;      // “求交集”子菜单
    QMenu *unionMenu;             // “求并集”子菜单
};
#endif // MAINWINDOW_H
