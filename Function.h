#ifndef FUNCTION_H
#define FUNCTION_H

#include <QWidget>
#include <QPoint>
#include <QVector>
#include <QMouseEvent>
#include <QPixmap> //用于背景图
#include <QPainterPath>
#include <list>
#include <optional>

//为 Weiler-Atherton 算法定义的顶点节点结构体
struct VertexNode {
    QPointF point;// 顶点坐标
    bool is_intersection = false;// 是否为交点
    std::list<VertexNode>::iterator neighbor;//对应另一个链表中交点的指针（配对点）
    bool is_entering = false;// 是否为进入交点（决定是否切换边界）
    bool processed = false;// 是否已被处理（用于封闭轮廓循环标记）
    double alpha = 0.0; // 插值位置（在原边段上的比例，用于排序）
};

//超前声明
struct Triangle;

class DrawingWidget : public QWidget
{
    Q_OBJECT // 宏，用于支持Qt的信号和槽机制

public:
    // 定义一个枚举来管理当前的工作模式
    enum Mode {
        IDLE,                   // 空闲模式
        ADD_POINTS_CONVEX_HULL, // 添加点以计算凸包
        DRAW_POLYGON,            // 绘制多边形（用于三角剖分或面积计算）
        DRAW_POLYGON_A,  // 计算交时的第一个多边形
        DRAW_POLYGON_B   // 计算交时的第二个多边形
    };

    // 【新增】为 Weiler-Atherton 算法定义操作类型
    enum BooleanOpType { Intersection, Union };

    explicit DrawingWidget(QWidget *parent = nullptr);//构造函数
    void setTask(const QString& task);//指定当前多边形绘制任务，比如 "triangulate" 或 "area"，由菜单项触发

signals:
    //当模式改变时，发射此信号并附带提示信息
    void modeChanged(const QString& message);
    void polygonsReady(bool isReady); //信号，通知主窗口两个多边形已准备好

public slots:
    // 公共槽函数
    void setMode(Mode newMode);
    void clearScreen();
    void performCalculation();

    void startAndrewConvexHull();
    void startGrahamConvexHull();

    //QPainterPath算法的槽函数
    void showIntersection_QPainterPath();
    void showUnion_QPainterPath();

    //Weiler算法的槽函数
    void showIntersection_Weiler();
    void showUnion_Weiler();

protected:
    // Qt事件处理函数，重写这些函数以响应鼠标和绘图事件
    void paintEvent(QPaintEvent *event) override;//重新绘制界面，负责显示点、边、凸包、多边形、三角剖分、面积等
    void mousePressEvent(QMouseEvent *event) override;//处理用户点击：左键添加点或顶点，右键触发计算

private:
    // --- 算法实现函数 ---
    void calculateConvexHull_Andrew(); //重命名
    void calculateConvexHull_Graham(); //格雷厄姆扫描法

    void calculateIntersectionAndUnion();

    void calculateBooleanOp_WeilerAtherton(BooleanOpType opType);

    void calculateTriangulation();
    void calculatePolygonArea();


    // --- 辅助函数 ---
    double crossProduct(const QPointF &p1, const QPointF &p2, const QPointF &p3);
    bool isPolygonEdge(const QPointF &a, const QPointF &b);
    bool isSimplePolygon(const QVector<QPointF> &poly);
    bool onSegment(const QPointF &a, const QPointF &b, const QPointF &c);
    bool segmentsIntersect(QPointF p1, QPointF p2, QPointF q1, QPointF q2);
    double computeAreaSign(const QVector<QPointF> &pts);
    std::optional<QPointF> getLineSegmentIntersection(QPointF p1, QPointF p2, QPointF p3, QPointF p4, double& out_alpha);
    bool isPointInsidePolygon(const QPointF& point, const QVector<QPointF>& polygon);

    // --- 成员变量 ---
    QVector<QPointF> polygonA;//计算交时的第一个多边形
    QVector<QPointF> polygonB;//计算交时的第二个多边形
    QVector<QPolygonF> intersectionPolygons; // 交集区域（可能有多个）
    QPainterPath unionPath; //直接存储并集的结果路径
    QPainterPath weilerResultPath;

    Mode currentMode;               // 当前的工作模式
    QString taskToPerform;          // 在DRAW_POLYGON模式下，具体要执行的任务 ("triangulate" 或 "area")
    QString convexHullAlgorithm;
    QPixmap m_background; //用于存储背景图片
    QString displayMode; //用于记录当前是显示交集 "intersection" 还是并集 "union"
    bool polygonsReadyForOperation = false;
    bool showGrid = false; //用于控制是否显示坐标网格

    // --- 几何数据容器 ---
    QVector<QPointF> points;         // 存储用户点击的点 (用于凸包)
    QVector<QPointF> convexHull;     // 存储计算出的凸包顶点
    QVector<QPointF> polygonVertices;// 存储用户绘制的多边形顶点
    QVector<Triangle> triangles;    // 存储剖分后的三角形
    QVector<QPolygonF> weilerResultPolygons;
    double polygonArea;             // 存储计算出的多边形面积
    int triangleCount = -1; //用于记录三角形数量，-1表示未计算
};

// 用于三角剖分的辅助结构体
struct Triangle {
    QPointF p1, p2, p3;
    Triangle(const QPointF &pt1, const QPointF &pt2, const QPointF &pt3)
        : p1(pt1), p2(pt2), p3(pt3) {}
    bool contains(const QPointF &pt) const;
};

#endif // FUNCTION_H
