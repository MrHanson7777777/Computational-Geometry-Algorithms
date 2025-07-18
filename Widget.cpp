#include "Widget.h"
#include "./ui_Widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)//初始化基类QWidget
    , ui(new Ui::Widget)//创建一个指向UI控件的指针
{
    ui->setupUi(this);
    /*
     * 实例化 UI 控件（按钮、文本框等）
     * 设置他们的位置、大小、属性
     * 连接到当前 Widget 界面上（this）
    */
}

Widget::~Widget()
{
    delete ui;
}
