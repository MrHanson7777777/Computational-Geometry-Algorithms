#include "Function.h"
#include <QPainter>
#include <QMessageBox>
#include <algorithm>
#include <QPainterPath>

/**
 * @brief DrawingWidget 类的构造函数
 * @param parent 父窗口部件指针
 * @details 初始化控件的基础设置，包括默认模式、面积初始值，并尝试从Qt资源系统加载背景图片。
 * 如果加载失败，则设置一个纯白色背景作为备用。
 */
DrawingWidget::DrawingWidget(QWidget *parent)
    : QWidget(parent), currentMode(IDLE), polygonArea(-1.0) // 初始化列表
//调用基类QWidget构造函数，建立图形窗口；初始化模式为IDLE空闲；初始化多边形面积为-1表示还未计算
{
    if (!m_background.load(":/images/background.jpg")) {
        qDebug() << "Failed to load background image!";
        // 如果加载失败，可以设置一个纯色背景作为备用
        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::white);
        setPalette(pal);

    }else{
        setAutoFillBackground(true);//自动填色开关
        QPalette pal = palette();//获取当前窗口的颜色方案（调色板）
        //将 setColor 修改为 setBrush，并使用加载的图片 m_background
        pal.setBrush(this->backgroundRole(), m_background);
        setPalette(pal);
    }
}

/**
 * @brief 设置当前要执行的任务类型
 * @param task 任务名称字符串 (例如 "area", "triangulate")
 * @details 此函数由主窗口的菜单项触发，用于告知绘图控件在进入 DRAW_POLYGON 模式后具体要执行哪种计算。
 * 特别地，当任务为 "area" 时，会激活背景网格的显示。
 */
void DrawingWidget::setTask(const QString &task)
{
    this->taskToPerform = task;

    if (task == "area") {// 如果任务是计算面积，就打开网格显示
        showGrid = true;
    }
}

/**
 * @brief 切换绘图控件的工作模式
 * @param newMode 要切换到的新模式 (来自 Mode 枚举)
 * @details 在切换模式前，会调用 clearScreen() 来重置画布和所有状态。
 * 然后根据新模式，通过发射 modeChanged 信号来更新主窗口状态栏的提示信息。
 */
void DrawingWidget::setMode(Mode newMode)
{
    /*在用户点击菜单项时被调用，负责完成几项关键任务：清空当前图形、更新模式状态、弹出提示指导用户下一步操作*/

    clearScreen();//清除所有绘图数据，包括：已添加的点（points）、多边形顶点（polygonVertices）、三角形剖分结果（triangles）、面积数据（polygonArea）
    currentMode = newMode;

    if (currentMode == ADD_POINTS_CONVEX_HULL) {
        taskToPerform = "convexHull";
        emit modeChanged("当前模式：计算凸包。请用左键添加点，右键或点击菜单执行计算。");
    } else if (currentMode == DRAW_POLYGON) {
        emit modeChanged("当前模式：绘制多边形。请用左键添加顶点，右键或点击菜单执行计算。");
    } else if (currentMode == DRAW_POLYGON_A) {
        emit modeChanged("当前模式：绘制多边形 A。请用左键添加顶点，右键完成。");
    } else if (currentMode == DRAW_POLYGON_B) {
        emit modeChanged("当前模式：绘制多边形 B。请用左键添加顶点，右键完成并计算交集。");
    }
}

/**
 * @brief 清除所有绘图数据并重置状态
 *
 * 此函数用于恢复绘图区域到初始状态，清除所有用户输入的图形元素，
 * 并重置程序的模式、算法选择、计算结果等。适用于重新开始或取消当前操作。
 *
 * 功能包括：
 * - 清除多边形、凸包、交并集、三角剖分等图形数据
 * - 重置交互模式和算法选择
 * - 通知主窗口禁用菜单项（通过信号）
 * - 刷新界面，恢复白底画布
 */

void DrawingWidget::clearScreen()
{
    currentMode = IDLE;//设置当前模式为空闲状态，停止所有绘图任务
    taskToPerform.clear();//清空任务标识，例如 "convexHull"、"area" 或 "triangulate"
    points.clear();//清除凸包计算的点集
    convexHull.clear();//清除凸包结果的点集
    convexHullAlgorithm.clear(); //重置算法选择

    polygonsReadyForOperation = false;//标记为“尚未准备好”状态，防止执行交并操作
    displayMode.clear();//用于记录当前是显示交集 "intersection" 还是并集 "union"
    emit polygonsReady(false); //在还没画好多边形时发射信号通知主窗口禁用交并菜单项
    polygonA.clear();
    polygonB.clear();
    intersectionPolygons.clear();
    unionPath.clear();

    polygonVertices.clear();//清除用户绘制的多边形顶点
    triangles.clear();//清除三角剖分生成的三角形列表
    polygonArea = -1.0;//重置面积值为无效状态（负值表示未计算）
    triangleCount = -1;//重置三角形数量
    showGrid = false;//重置网格显示状态
    update(); // 触发paintEvent，重新绘制，更新界面，使得所有图形都会从屏幕上消失，恢复成白底画布
}

/**
 * @brief 执行当前模式下配置的计算任务
 * @details 这是一个总控函数，由“执行计算”菜单或右键点击触发。
 * 它会根据 currentMode 和 taskToPerform/convexHullAlgorithm 的值，
 * 分发到相应的具体算法函数（如 calculateConvexHull_Andrew, calculateTriangulation 等）。
 * 在执行前，会进行必要的合法性检查（如顶点数量、是否自相交）。
 */
void DrawingWidget::performCalculation()
{
    if (currentMode == ADD_POINTS_CONVEX_HULL) {
        //不再弹窗，而是直接根据成员变量做出选择
        if (convexHullAlgorithm == "Andrew") {
            calculateConvexHull_Andrew();
        } else if (convexHullAlgorithm == "Graham") {
            calculateConvexHull_Graham();
        } else {
            // 提供一个备用提示，以防用户未通过菜单选择
            QMessageBox::information(this, "提示", "请先从“算法”->“计算凸包”菜单中选择一种具体算法。");
            return;
        }
    }
    else if (currentMode == DRAW_POLYGON) {
        // 检查点数是否足够
        if (polygonVertices.size() < 3) {
            QMessageBox::warning(this, "错误", "多边形至少需要 3 个顶点！");
            return;
        }

        // 在这里进行最终的、完整的合法性检查
        // 注意：现在 isSimplePolygon 内部已经包含了对零长度边的检查
        if (!isSimplePolygon(polygonVertices)) {
            QMessageBox::warning(this, "错误", "多边形存在自相交或不合法（如零长度边），无法计算！");
            return;
        }

        // 在mainwindow中根据触发的Action来设置taskToPerform
        if (taskToPerform == "triangulate") {
            calculateTriangulation();
        } else if (taskToPerform == "area") {
            calculatePolygonArea();
        }

        //处理交集计算的逻辑
        else if (currentMode == DRAW_POLYGON_B && polygonB.size() >= 3) {
            calculateIntersectionAndUnion();
        }
    }
    update();
}

/**
 * @brief 响应菜单，开始准备进行 Andrew 算法凸包计算
 * @details 将模式设置为 ADD_POINTS_CONVEX_HULL，并记录用户的算法选择。
 */
void DrawingWidget::startAndrewConvexHull()
{
    setMode(ADD_POINTS_CONVEX_HULL);
    convexHullAlgorithm = "Andrew";
    emit modeChanged("当前模式：计算凸包 (Andrew)。请添加点后点击“执行计算”。");
}

/**
 * @brief 响应菜单，开始准备进行 Graham 算法凸包计算
 * @details 将模式设置为 ADD_POINTS_CONVEX_HULL，并记录用户的算法选择。
 */
void DrawingWidget::startGrahamConvexHull()
{
    setMode(ADD_POINTS_CONVEX_HULL);
    convexHullAlgorithm = "Graham";
    emit modeChanged("当前模式：计算凸包 (Graham)。请添加点后点击“执行计算”。");
}

/**
 * @brief 核心绘图事件处理函数
 * @param event 绘图事件指针
 * @details 此函数在每次界面需要刷新时（如窗口大小改变、调用 update()）被自动调用。
 * 它负责根据当前存储的几何数据（点、多边形、凸包、交并集结果等），
 * 使用 QPainter 将所有可见元素绘制到屏幕上。
 */
void DrawingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);//为了避免未使用变量编译警告
    QPainter painter(this);//Qt的绘图类，绑定到当前控件 DrawingWidget
    painter.setRenderHint(QPainter::Antialiasing, true); //开启抗锯齿，让线条和点更平滑，避免出现锯齿感，提高绘图质量

    //绘制背景图片
    if (!m_background.isNull()) {
        painter.drawPixmap(this->rect(), m_background);
    }

    if (showGrid) {
        const int gridSize = 50; // 定义网格大小为 50 像素
        const QColor gridColor = QColor(Qt::white).darker(150); // 设置一个深灰色作为网格颜色

        QPen gridPen(gridColor, 1, Qt::DotLine); // 网格线使用虚线
        painter.setPen(gridPen);

        // 绘制垂直线和X轴坐标
        for (int x = gridSize; x < this->width(); x += gridSize) {
            painter.drawLine(x, 0, x, this->height());
            painter.drawText(x - 20, 15, QString::number(x));
        }

        // 绘制水平线和Y轴坐标
        for (int y = gridSize; y < this->height(); y += gridSize) {
            painter.drawLine(0, y, this->width(), y);
            painter.drawText(5, y + 15, QString::number(y));
        }
    }

    // 1. 绘制用户点击的点 (用于凸包)
    painter.setBrush(Qt::blue);//画这些点用蓝色填充
    painter.setPen(Qt::NoPen);//不要描边，只画填充圆点
    int index = 1;
    for (const QPointF &p : points) {
        painter.drawEllipse(p, 3, 3);//每个点被绘制为半径 3 像素的圆点
        /*
         * 第一个参数 p：表示椭圆的中心点位置，也就是你鼠标点击的坐标
         * 第二、第三个参数 3, 3代表rx: 水平方向半径（半宽度）；ry: 垂直方向半径（半高度）
        */
        painter.setPen(Qt::white); // 用白色文字
        painter.drawText(p.x() + 5, p.y() - 5, QString("P%1").arg(index++)); // 点名
        painter.setPen(Qt::NoPen);
    }

    // 2. 绘制多边形顶点和边
    if (!polygonVertices.isEmpty()) {
        painter.setBrush(Qt::red);
        int polyIndex = 1;
        for (const QPointF &p : polygonVertices) {
            painter.drawEllipse(p, 5, 5);
            painter.setPen(Qt::white);
            painter.drawText(p.x() + 5, p.y() - 5, QString("P%1").arg(polyIndex++));
            painter.setPen(Qt::NoPen);
        }

        //如果已完成三角剖分，则边界也画虚线，否则画实线
        if (triangles.isEmpty()) {
            painter.setPen(QPen(Qt::blue, 2)); // 正常实线
        } else {
            painter.setPen(QPen(Qt::blue, 2, Qt::DashLine)); // 三角剖分后，边界也为虚线
        }
        painter.drawPolyline(polygonVertices);

        if (polygonVertices.size() > 2) {
            painter.drawLine(polygonVertices.last(), polygonVertices.first());
        }
    }

    // 3. 绘制凸包 (红色)
    if (!convexHull.isEmpty()) {
        painter.setPen(QPen(Qt::red, 2));
        painter.setBrush(Qt::NoBrush);//没有填充颜色
        painter.drawPolygon(convexHull);//自动将首尾连成闭环封口
    }

    // 4.绘制多边形 A
    if (!polygonA.isEmpty()) {
        painter.setPen(Qt::blue);
        painter.drawPolyline(polygonA);

        //绘制闭合线时检查顶点数
        if (polygonA.size() > 2) {
            painter.drawLine(polygonA.last(), polygonA.first());
        }

        //显示 P1, P2...
        painter.setPen(Qt::white);
        for (int i = 0; i < polygonA.size(); ++i) {
            painter.drawText(polygonA[i].x() + 5, polygonA[i].y() - 5,
                             QString("P%1").arg(i + 1));
        }
    }

    // 5.绘制多边形 B
    if (!polygonB.isEmpty()) {
        painter.setPen(Qt::red);
        painter.drawPolyline(polygonB);

        //绘制闭合线时检查顶点数
        if (polygonB.size() > 2) {
            painter.drawLine(polygonB.last(), polygonB.first());
        }

        //显示 Q1, Q2...
        painter.setPen(Qt::white);
        for (int i = 0; i < polygonB.size(); ++i) {
            painter.drawText(polygonB[i].x() + 5, polygonB[i].y() - 5,
                             QString("Q%1").arg(i + 1));
        }
    }

    // 6.绘制交并结果
    if (polygonsReadyForOperation) {
        painter.setPen(Qt::NoPen);

        // QPainterPath 法 (不变)
        if (displayMode == "intersection_qpath") {
            painter.setBrush(QColor(139, 69, 19, 150));
            for (const QPolygonF &poly : intersectionPolygons) painter.drawPolygon(poly);
        } else if (displayMode == "union_qpath") {
            painter.setBrush(QColor(0, 255, 0, 150));
            painter.drawPath(unionPath);
        }
        // Weiler-Atherton 法
        else if (displayMode == "intersection_weiler" || displayMode == "union_weiler") {
            if (displayMode == "intersection_weiler") painter.setBrush(QColor(139, 69, 19, 150));
            else painter.setBrush(QColor(0, 255, 0, 150));

            // 【核心修正】使用 QPainterPath 来正确绘制可能带孔洞的图形
            QPainterPath resultPath;
            // 将所有找到的轮廓（包括外边界和内边界/孔洞）都添加到路径中
            for (const QPolygonF &poly : weilerResultPolygons) {
                resultPath.addPolygon(poly);
            }
            // 使用 WindingFill 规则，QPainterPath 会自动识别出孔洞并正确绘制
            resultPath.setFillRule(Qt::WindingFill);
            painter.drawPath(resultPath);
        }
    }

    // 7. 绘制三角剖分 (虚线)
    if (!triangles.isEmpty()) {
        //步骤1：先用半透明蓝色填充所有三角形
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 255, 60)); // 蓝色，60/255 的透明度
        for (const auto& t : triangles) {
            QPointF points[3] = {QPointF(t.p1), QPointF(t.p2), QPointF(t.p3)};
            painter.drawPolygon(points, 3);
        }

        //步骤2：绘制加粗的虚线边界
        painter.setPen(QPen(Qt::darkGray, 2, Qt::DashLine)); // 宽度从1改为2
        for (int k = 0; k < triangles.size(); ++k) {
            Triangle t = triangles[k];
            // 只画非边界的虚线
            if (!isPolygonEdge(t.p1, t.p2)) painter.drawLine(t.p1, t.p2);
            if (!isPolygonEdge(t.p2, t.p3)) painter.drawLine(t.p2, t.p3);
            if (!isPolygonEdge(t.p3, t.p1)) painter.drawLine(t.p3, t.p1);
        }
    }

    // 8. 显示多边形面积
    if (polygonArea >= 0) {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(20, 30, QString("面积: %1").arg(polygonArea, 0, 'f', 2));
    }

    // 9. 显示三角形数量
    if (triangleCount >= 0) {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        // 将文本绘制在“面积”下方，Y坐标增加一些
        painter.drawText(20, 50, QString("三角形数量: %1").arg(triangleCount));
    }
}

/**
 * @brief 鼠标点击事件处理函数
 * @param event 鼠标事件指针，包含了点击位置和按钮类型
 * @details - 左键点击：根据当前模式 (currentMode)，将点添加到相应的点集
 * （如 `points` 用于凸包，`polygonA` 用于交并集）。
 * - 右键点击：触发当前操作的结束或计算。例如，结束多边形A的绘制并切换到B，
 * 或在点/多边形绘制完成后调用 performCalculation() 执行计算。
 */
void DrawingWidget::mousePressEvent(QMouseEvent *event)
{
    // 左键点击添加点
    if (event->button() == Qt::LeftButton) {
        if (currentMode == DRAW_POLYGON_A) {
            polygonA.append(event->pos());
        } else if (currentMode == DRAW_POLYGON_B) {
            polygonB.append(event->pos());
        } else if (currentMode == DRAW_POLYGON) {
            polygonVertices.append(event->pos());
        } else if (currentMode == ADD_POINTS_CONVEX_HULL) {
            points.append(event->pos());
        }
        update();
        return; // 添加完点后直接返回
    }

    // 右键点击结束绘制并验证
    if (event->button() == Qt::RightButton) {
        // 结束多边形A的绘制
        if (currentMode == DRAW_POLYGON_A && polygonA.size() >= 3) {
            if (!isSimplePolygon(polygonA)) {
                QMessageBox::warning(this, "错误", "多边形 A 存在自相交，请重新绘制！");
                clearScreen(); // 清屏重置
            } else {
                currentMode = DRAW_POLYGON_B; // 切换到绘制B的模式
                emit modeChanged("多边形 A 合法。请用左键添加顶点绘制多边形 B，右键完成。");
            }
        }
        // 结束多边形B的绘制
        else if (currentMode == DRAW_POLYGON_B && polygonB.size() >= 3) {
            if (!isSimplePolygon(polygonB)) {
                QMessageBox::warning(this, "错误", "多边形 B 存在自相交，请重新绘制！");
                clearScreen(); // 清屏重置
            } else {
                currentMode = IDLE; // 两个多边形都合法，进入空闲模式
                polygonsReadyForOperation = true; // 标记已准备好
                emit polygonsReady(true); // 发射信号，启用主窗口的菜单项
                emit modeChanged("两个多边形均合法。请从菜单选择求交集或并集。");
            }
        }
        // 处理其他模式的右键点击（例如三角剖分）
        else if (currentMode == DRAW_POLYGON && polygonVertices.size() >= 3) {
            if (!isSimplePolygon(polygonVertices)) {
                QMessageBox::warning(this, "错误", "多边形存在自相交，无法计算！");
                return;
            }
            performCalculation();
        }
        // ... 其他模式如凸包...
        else if (currentMode == ADD_POINTS_CONVEX_HULL && points.size() >= 3) {
            performCalculation();
        }
    }
}

// =================================================================
//                              算法实现
// =================================================================
/**
 * @brief 使用 Andrew's Monotone Chain 算法计算点集的凸包。
 * @details 算法首先按X坐标对所有点进行排序，然后分别构建上凸包和下凸包，最后合并得到最终结果。
 * 这是一个高效且稳健的凸包算法。
 * @note 此函数直接操作成员变量 `points` 和 `convexHull`。
 * @complexity O(n log n)，主要瓶颈在于排序。
 */
void DrawingWidget::calculateConvexHull_Andrew()
{
    if (points.size() < 3) return;

    // 1. 按 x 坐标排序，x 相同则按 y 坐标排序
    QVector<QPointF> sortedPoints = points;
    std::sort(sortedPoints.begin(), sortedPoints.end(), [](const QPointF &a, const QPointF &b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });

    QVector<QPointF> upper, lower;

    // 2. 构建下凸包
    for (const QPointF &p : sortedPoints) {
        while (lower.size() >= 2 && crossProduct(lower[lower.size()-2], lower.last(), p) <= 0) {
            lower.pop_back();
        }
        lower.push_back(p);
    }

    // 3. 构建上凸包
    for (int i = sortedPoints.size() - 1; i >= 0; --i) {
        const QPointF &p = sortedPoints[i];
        while (upper.size() >= 2 && crossProduct(upper[upper.size()-2], upper.last(), p) <= 0) {
            upper.pop_back();
        }
        upper.push_back(p);
    }

    // 4. 合并上下凸包
    convexHull = lower;
    convexHull.pop_back(); // 移除重复的终点
    convexHull += upper;
    convexHull.pop_back(); // 移除重复的起点

    currentMode = IDLE;
    //交互式绘图窗口，所以它有不同的模式控制
    /*
     *ADD_POINTS_CONVEX_HULL → 用户正在添加点，用于构造凸包
     *DRAW_POLYGON → 用户在画多边形，用于面积或三角剖分
     *IDLE → 什么都不干了，等待下一步指令
    */
}

/**
 * @brief 使用 Graham Scan (格雷厄姆扫描法) 计算点集的凸包。
 * @details 算法首先找到Y坐标最小的点作为锚点，然后将其余点按与锚点的极角排序，最后通过栈操作构建出凸包。
 * @note 此函数在 `points` 的副本上操作，不会修改原始点集。
 * @complexity O(n log n)，主要瓶颈在于极角排序。
 */
void DrawingWidget::calculateConvexHull_Graham()
{
    if (points.size() < 3) return;

    // 创建 points 的副本，所有操作都在这个副本上进行
    QVector<QPointF> tempPoints = points;

    // 1. 找到Y坐标最小的点（P0）
    int minY_idx = 0;
    for (int i = 1; i < tempPoints.size(); ++i) {
        if (tempPoints[i].y() < tempPoints[minY_idx].y() ||
            (tempPoints[i].y() == tempPoints[minY_idx].y() && tempPoints[i].x() < tempPoints[minY_idx].x())) {
            minY_idx = i;
        }
    }
    std::swap(tempPoints[0], tempPoints[minY_idx]);
    QPointF p0 = tempPoints[0];

    // 2. 将其他点根据与P0的极角进行排序
    std::sort(tempPoints.begin() + 1, tempPoints.end(), [&](const QPointF& a, const QPointF& b) {
        double order = crossProduct(p0, a, b);

        // 处理共线情况
        if (std::abs(order) < 1e-9) {
            //直接手动计算距离的平方进行比较
            // 不再使用不存在的 lengthSquared() 函数
            double distSqA = (p0.x() - a.x()) * (p0.x() - a.x()) + (p0.y() - a.y()) * (p0.y() - a.y());
            double distSqB = (p0.x() - b.x()) * (p0.x() - b.x()) + (p0.y() - b.y()) * (p0.y() - b.y());
            return distSqA < distSqB; // 距离近的排在前面
        }

        // 叉积 > 0 表示 p0->a 在 p0->b 的逆时针方向
        return order > 0;
    });

    // 3. 构建凸包
    convexHull.clear();
    if (tempPoints.size() < 3) {
        convexHull = tempPoints;
        currentMode = IDLE;
        return;
    }

    convexHull.push_back(tempPoints[0]);
    convexHull.push_back(tempPoints[1]);

    for (int i = 2; i < tempPoints.size(); ++i) {
        while (convexHull.size() > 1 &&
               crossProduct(convexHull[convexHull.size()-2], convexHull.last(), tempPoints[i]) <= 0) {
            convexHull.pop_back();
        }
        convexHull.push_back(tempPoints[i]);
    }

    currentMode = IDLE;
}

/**
 * @brief 使用 Qt 内置的 QPainterPath 计算两个多边形的交集和并集。
 * @details 这是最高效、最可靠的矢量布尔运算方法。
 * 它将多边形转换为 QPainterPath 对象，然后直接调用其 intersected() 和 united() 方法。
 * @note 结果分别存储在成员变量 `intersectionPolygons` 和 `unionPath` 中。
 */
void DrawingWidget::calculateIntersectionAndUnion()
{
    intersectionPolygons.clear();
    unionPath.clear();

    //将多边形 (存储为点列表)转换为 Qt内部的高级图形对象
    QPainterPath pathA, pathB;
    pathA.addPolygon(QPolygonF(polygonA));
    pathB.addPolygon(QPolygonF(polygonB));

    //直接调用内置的布尔运算函数
    QPainterPath inter = pathA.intersected(pathB);
    QPainterPath uni = pathA.united(pathB);

    //存储计算结果以供后续绘制
    intersectionPolygons = inter.toSubpathPolygons();
    unionPath = uni; //直接保存QPainterPath 对象
}

/**
 * @brief 响应菜单点击，使用 QPainterPath 法计算并显示交集
 * @details 设置当前的显示模式为 "intersection_qpath"，
 * 然后调用核心的布尔运算函数，最后刷新屏幕以展示结果。
 */
void DrawingWidget::showIntersection_QPainterPath()
{
    displayMode = "intersection_qpath";
    calculateIntersectionAndUnion();
    update();
}

/**
 * @brief 响应菜单点击，使用 QPainterPath 法计算并显示并集
 * @details 设置当前的显示模式为 "union_qpath"，
 * 然后调用核心的布尔运算函数，最后刷新屏幕以展示结果。
 */
void DrawingWidget::showUnion_QPainterPath()
{
    displayMode = "union_qpath";
    calculateIntersectionAndUnion();
    update();
}

/**
 * @brief 响应菜单点击，使用 Weiler-Atherton 算法计算并显示交集
 * @details 设置当前的显示模式为 "intersection_weiler"，
 * 然后调用 Weiler-Atherton 核心算法，并传入 Intersection 操作类型，
 * 最后刷新屏幕以展示结果。
 */
void DrawingWidget::showIntersection_Weiler()
{
    displayMode = "union_weiler";
    calculateBooleanOp_WeilerAtherton(Intersection);
    update();
}

/**
 * @brief 响应菜单点击，使用 Weiler-Atherton 算法计算并显示并集
 * @details 设置当前的显示模式为 "union_weiler"，
 * 然后调用 Weiler-Atherton 核心算法，并传入 Union 操作类型，
 * 最后刷新屏幕以展示结果。
 */
void DrawingWidget::showUnion_Weiler()
{
    displayMode = "intersection_weiler";
    calculateBooleanOp_WeilerAtherton(Union);
    update();
}

/**
 * @brief 计算两条线段 p1p2 和 p3p4 的交点
 * @details 此函数通过求解两个线段参数方程组成的线性方程组来找到交点。
 * 它首先计算系数行列式 `det`，如果 `det` 接近于零，则线段平行或共线，无交点。
 * 否则，解出参数 `t` 和 `u`。只有当 `t` 和 `u` 都严格在 (0, 1) 区间内时，
 * 交点才位于两条线段的内部，此时函数返回交点坐标。
 *
 * @param p1 线段1的起点
 * @param p2 线段1的终点
 * @param p3 线段2的起点
 * @param p4 线段2的终点
 * @param out_alpha [out] 如果相交，此参数将存储交点在线段 p1p2 上的比例位置 (t值)
 * @return std::optional<QPointF> 如果线段严格相交，则返回交点；否则返回 std::nullopt。
 */
std::optional<QPointF> DrawingWidget::getLineSegmentIntersection(QPointF p1, QPointF p2, QPointF p3, QPointF p4, double& out_alpha) {
    //设置浮点误差容忍值，避免因为微小误差误判“相交”
    const double EPSILON = 1e-9;
    //计算行列式，相当于向量叉积：(p2−p1) × (p4−p3)，如果 det = 0 → 两线段平行或重合
    double det = (p2.x() - p1.x()) * (p4.y() - p3.y()) - (p2.y() - p1.y()) * (p4.x() - p3.x());
    //如果行列式接近 0 → 视为平行，不相交 → 返回空值
    if (std::abs(det) < EPSILON)
        return std::nullopt;

    //点I(t) = P₁ + t * (P₂ - P₁) = P₃ + u * (P₄ - P₃)
    //x 坐标: P₁.x + t * (P₂.x - P₁.x) = P₃.x + u * (P₄.x - P₃.x)
    //y 坐标: P₁.y + t * (P₂.y - P₁.y) = P₃.y + u * (P₄.y - P₃.y)
    //使用克莱姆法则计算
    double t = ((p3.x() - p1.x()) * (p4.y() - p3.y()) - (p3.y() - p1.y()) * (p4.x() - p3.x())) / det;
    double u = -((p2.x() - p1.x()) * (p3.y() - p1.y()) - (p2.y() - p1.y()) * (p3.x() - p1.x())) / det;

    //最后检验
    if (t > EPSILON && t < 1.0 - EPSILON && u > EPSILON && u < 1.0 - EPSILON) {
        //检查 t 和 u 是否都严格在 (0, 1) 的开区间内
        out_alpha = t;
        return QPointF(p1.x() + t * (p2.x() - p1.x()), p1.y() + t * (p2.y() - p1.y()));
    }
    return std::nullopt;
    //计算两条线段的交点，如果确实相交，就返回交点；否则返回“空值”（表示没有交点）
}

/**
 * @brief 使用射线法（Ray Casting）判断一个点是否在多边形内部
 *
 * @details 从测试点向右发射一条水平射线，然后统计这条射线与多边形边的交点数量。
 * 如果交点数量为奇数，则点在多边形内部；如果为偶数，则点在外部。
 * 这是一个处理非凸多边形的经典算法。
 *
 * @param point 要测试的点
 * @param polygon 多边形的顶点列表（应为封闭的）
 * @return bool 如果点在多边形内部，返回 true；否则返回 false
 * @complexity O(n)，其中 n 是多边形的顶点数。
 */
bool DrawingWidget::isPointInsidePolygon(const QPointF& point, const QVector<QPointF>& polygon)
{
    /*
    从测试点 向右画一条水平射线。
    如果这条射线与多边形的边相交奇数次，点在多边形内部。
    如果相交偶数次，点在多边形外部。
    穿越规则：每次交叉会“切换”内外状态，奇数次意味着从外进入后停留在内。
    */
    bool inside = false;
    int n = polygon.size();
    if (n < 3) return false;//如果多边形顶点少于3个，不是合法多边形，直接返回 false

    //循环遍历多边形的每一条边，时间复杂度O(n)
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const QPointF& p_i = polygon[i];
        const QPointF& p_j = polygon[j];

        //检查点的Y坐标是否在当前边的Y坐标范围之内
        bool y_intersect = ((p_i.y() > point.y()) != (p_j.y() > point.y()));

        if (y_intersect) {
            //计算从点向右发出的水平射线与当前边的交点的X坐标
            //这是一个基于相似三角形的标准线性插值公式（占比，相似三角形）
            double x_intersect = (p_j.x() - p_i.x()) * (point.y() - p_i.y()) / (p_j.y() - p_i.y()) + p_i.x();

            //如果交点的X坐标在点的右侧，说明射线穿过了这条边
            if (point.x() < x_intersect) {
                //每穿过一次，就切换一次内外状态
                inside = !inside;
            }
        }
    }
    return inside;
}

/**
 * @brief 使用 Weiler–Atherton 算法计算两个多边形的布尔运算（交集或并集）。
 * @param opType 指定要执行的操作是 Intersection 还是 Union。
 * @details 算法通过构建两个多边形的增强链表，找到所有交点，并根据“进入/穿出”规则
 * 在两个链表之间“穿梭”，最终缝合出结果多边形。可以正确处理多区域和带孔洞的情况。
 * @note 结果存储在成员变量 `weilerResultPolygons` 中。
 * @complexity O(I*log(I) + (n+m+I))，其中 I 是交点数，最坏可达 O(n*m)。
 */
void DrawingWidget::calculateBooleanOp_WeilerAtherton(BooleanOpType opType) {
    //清空旧数据并进行有效性检查
    weilerResultPolygons.clear();
    if (polygonA.size() < 3 || polygonB.size() < 3) return;

    QVector<QPointF> polyA = this->polygonA;
    QVector<QPointF> polyB = this->polygonB;

    //确保两个多边形都是逆时针顺序，时间复杂度O(n+m)
    if (computeAreaSign(polyA) > 0) std::reverse(polyA.begin(), polyA.end());
    if (computeAreaSign(polyB) > 0) std::reverse(polyB.begin(), polyB.end());

    //构建增强链表，时间复杂度O(n+m)
    std::list<VertexNode> listA, listB;
    for(const auto& p : polyA) listA.push_back({p});
    for(const auto& p : polyB) listB.push_back({p});
    //每个节点存放：point：顶点坐标

    //寻找所有交点，并插入链表，时间复杂度O(n*m)
    for (auto itA = listA.begin(); itA != listA.end(); ++itA) {
        //遍历A中每个点
        auto next_itA = (std::next(itA) == listA.end()) ? listA.begin() : std::next(itA);//处理首尾点
        for (auto itB = listB.begin(); itB != listB.end(); ++itB) {
            //遍历B中每个点
            auto next_itB = (std::next(itB) == listB.end()) ? listB.begin() : std::next(itB);//处理首尾点

            double alpha;
            if (auto intersect_pt = getLineSegmentIntersection(itA->point, next_itA->point, itB->point, next_itB->point, alpha)) {
                //找到交点，将交点插入到链表中
                auto nodeA = listA.insert(next_itA, {intersect_pt.value(), true, {}, false, false, alpha});
                auto nodeB = listB.insert(next_itB, {intersect_pt.value(), true, {}, false, false, 0});

                nodeA->neighbor = nodeB;
                nodeB->neighbor = nodeA;

                //事先规定A和B都是逆时针，这里看交叉点处B多边形的方向在A多边形方向的左边还是右边
                double cross = crossProduct({0,0}, next_itA->point - itA->point, next_itB->point - itB->point);
                nodeA->is_entering = cross > 0;//大于零就是左侧，左侧就是内侧，进入
                nodeB->is_entering = cross < 0;
            }
        }
    }

    //遍历与缝合，找出结果多边形，时间复杂度 (O(n + m + I))
    for (auto it_start = listA.begin(); it_start != listA.end(); ++it_start) {
        if (!it_start->is_intersection || it_start->processed) continue;
        //is_intersection：必须是交点（否则跳过），已经处理过，就不能再用（避免重复构造）

        bool is_union = (opType == Union);
        if (is_union == it_start->is_entering) continue;
        //如果是 并集，则必须从 退出交点开始。
        //如果是 交集，则必须从 进入交点开始

        QPolygonF current_result;//当前路径的点集合
        auto current_iter = it_start;//当前访问的节点
        auto* current_list = &listA;//当前在哪条链表（A 或 B）
        int loop_guard = 0;//防止死循环（因为链表是环形的）
        int max_loops = listA.size() + listB.size() + 1; // 增加保护

        do {
            if (++loop_guard > max_loops) break;//防止死循环

            current_iter->processed = true;//标记为 processed，避免重复使用
            if(current_iter->is_intersection) current_iter->neighbor->processed = true;
            //如果是交点，它在另一条链表的 neighbor 也要标记

            current_result.append(current_iter->point);//把当前点加入结果多边形

            if (current_iter->is_intersection) {
                //如果走到交点，需要考虑是否转换
                if (is_union != current_iter->is_entering) {
                    // 根据并集/交集规则决定是否切换链表
                    //并集、往外退
                    //交集、往里进
                    //这两种情况换
                    current_iter = current_iter->neighbor;//转换调到另一个多边形
                    current_list = (current_list == &listA) ? &listB : &listA;//换链表
                }
            }

            current_iter++;//看下一个点
            if (current_iter == current_list->end()) {//衔接开头
                current_iter = current_list->begin();
            }
        } while (current_iter != it_start && (!it_start->is_intersection || current_iter != it_start->neighbor));
        //结束条件是如果回到起点，结束；如果是交点，但走回了它的邻居，说明闭环完成，结束。
        if (current_result.size() > 2) {
            //存结果
            weilerResultPolygons.push_back(current_result);
        }
    }

    //处理无交点的特殊情况（包含或相离），时间复杂度(O(n+m))
    if (weilerResultPolygons.empty() && !polyA.isEmpty() && !polyB.isEmpty()) {
        //没有交点导致没有结果且非空合法
        bool a_in_b = isPointInsidePolygon(polyA[0], polyB);//A 的一个点是否在 B 内部
        bool b_in_a = isPointInsidePolygon(polyB[0], polyA);//B 的一个点是否在 A 内部

        if (opType == Intersection) {
            //要求交集
            if (a_in_b)
                weilerResultPolygons.push_back(QPolygonF(polyA));//A在B中，交集为A
            else if (b_in_a)
                weilerResultPolygons.push_back(QPolygonF(polyB));//B在A中，交集为B
        } else { // Union
            if (a_in_b)
                weilerResultPolygons.push_back(QPolygonF(polyB));//A在B中，并集为B
            else if (b_in_a)
                weilerResultPolygons.push_back(QPolygonF(polyA));//B在A中，并集为A
            else {
                //如果互不包含，并集是 A + B（两个分离区域）
                weilerResultPolygons.push_back(QPolygonF(polyA));
                weilerResultPolygons.push_back(QPolygonF(polyB));
            }
        }
    }
}

/**
 * @brief 使用 Ear Clipping (耳朵裁剪) 算法对简单多边形进行三角剖分。
 * @details 算法循环寻找“耳朵”（一个凸顶点及其相邻两点组成的、内部不包含其他顶点的三角形），
 * 切下耳朵，直到多边形退化为一个三角形。包含了对复杂凹多边形的容错处理机制。
 * @note 结果存储在成员变量 `triangles` 中。
 * @complexity O(n^2) 在最坏情况下。
 */
void DrawingWidget::calculateTriangulation()
{

    //基础检查和预处理
    //至少三个点
    if (polygonVertices.size() < 3) {
        // 多边形至少需要3个顶点，否则无法构成三角形
        QMessageBox::warning(this, "错误", "无法剖分：顶点数不足 3 个！");
        return;
    }

    //简单多边形
    if (!isSimplePolygon(polygonVertices)) {
        // 自相交或零边长度等非法情况 → 剖分逻辑不能保证正确性
        QMessageBox::warning(this, "错误", "无法剖分：多边形不合法！");
        return;
    }

    //备份各点
    QVector<QPointF> remaining = polygonVertices;
    triangles.clear(); //清空之前的剖分结果

    if (!remaining.isEmpty() && remaining.first() == remaining.last()) {
        //首尾重复点，那么移除最后一个点，避免重复边
        remaining.removeLast();
    }

    //确定点序方向为逆时针（计算有向面积符号）
    double areaSign = 0.0;
    for (int i = 0; i < remaining.size(); ++i) {
        QPointF p1 = remaining[i];
        QPointF p2 = remaining[(i + 1) % remaining.size()];
        areaSign += (p1.x() * p2.y() - p2.x() * p1.y()); //鞋带公式核心部分
    }

    if (areaSign < 0) {
        // 如果面积是负值，说明顶点是顺时针排列（在 Qt 坐标中方向相反）
        // 为了统一处理，手动翻转点序
        std::reverse(remaining.begin(), remaining.end());
    }

    //耳切主循环
    int n = remaining.size();
    int attempts = 0;
    const int maxAttempts = n * 2; //防止死循环设置的最大容忍尝试次数

    while (remaining.size() > 3 && attempts < maxAttempts) {
        //每次剖分删除一个顶点，至多 n-3 次迭代，但在退化情况最多允许2n次尝试

        // 遍历所有三连顶点，尝试找到一个耳朵
        for (int i = 0; i < remaining.size(); ++i) {
            //外层迭代O(n)
            QPointF p1 = remaining[i];
            QPointF p2 = remaining[(i + 1) % remaining.size()];
            QPointF p3 = remaining[(i + 2) % remaining.size()];

            // 判断 p2 是否是凸角（改进：允许共线，提升容忍度）
            long long cross = crossProduct(p1, p2, p3);
            if (cross > 0) { //凸角
                Triangle ear(p1, p2, p3);
                bool isValidEar = true;

                // 遍历剩余顶点，判断是否有点在耳朵三角形内
                for (int j = 0; j < remaining.size(); ++j) {
                    //内层检查所有点是否在三角形内O(n)
                    if (j != i && j != (i + 1) % remaining.size() && j != (i + 2) % remaining.size()) {
                        QPointF pt = remaining[j];
                        if (ear.contains(pt)) {
                            isValidEar = false; //三角形内有点，不能剪耳朵
                            break;
                        }
                    }
                }

                if (isValidEar) {
                    //找到一个合法耳朵，那么添加到结果、移除中间顶点
                    triangles.push_back(ear);
                    remaining.remove((i + 1) % remaining.size());
                    attempts = 0; //重置尝试计数器
                    break;
                }
            }
        }
    }

    //处理最后剩余三角形
    if (remaining.size() == 3) {
        triangles.push_back(Triangle(remaining[0], remaining[1], remaining[2]));
        triangleCount = triangles.size(); //更新总数
        currentMode = IDLE;
    } else if (attempts >= maxAttempts) {
        //最大尝试次数触发 → 算法终止，并清空结果
        QMessageBox::warning(this, "错误", "无法剖分：算法无法继续执行！");
        triangles.clear();
        return;
    }
}

/**
 * @brief 使用 Shoelace (鞋带) 公式计算多边形面积。
 * @details 通过计算多边形顶点坐标的叉积和来得到面积。
 * @note 结果存储在成员变量 `polygonArea` 中。
 * @complexity O(n)
 */
void DrawingWidget::calculatePolygonArea()
{
    double area = 0.0;
    for (int i = 0; i < polygonVertices.size(); ++i) {
        //遍历所有点
        QPointF p1 = polygonVertices[i];
        QPointF p2 = polygonVertices[(i + 1) % polygonVertices.size()]; // 最后一个顶点和第一个顶点相连
        area += (double)(p1.x() * p2.y() - p2.x() * p1.y());
    }
    polygonArea = std::abs(area) / 2.0;
    currentMode = IDLE;
}

// =================================================================
//                            辅助函数
// =================================================================

/**
 * @brief 计算三点之间的二维叉积（向量 p1→p2 与 p1→p3 的有向面积）
 *
 * 此函数用于判断由三个点构成的旋转方向：
 * - 返回值 > 0：表示左转（逆时针方向）
 * - 返回值 < 0：表示右转（顺时针方向）
 * - 返回值 = 0：表示三点共线
 *
 * @param p1 第一个点（参考原点）
 * @param p2 第二个点（构成向量 p1→p2）
 * @param p3 第三个点（构成向量 p1→p3）
 * @return double 类型叉积结果
 *
 * @note 此函数常用于凸包、三角剖分、线段相交判断等几何算法中
 */
double DrawingWidget::crossProduct(const QPointF &p1, const QPointF &p2, const QPointF &p3)
{
    return (p2.x() - p1.x()) * (p3.y() - p1.y())-(p2.y() - p1.y()) * (p3.x() - p1.x());
}

/**
 * @brief 检查线段 (a, b) 是否为多边形的外边界。
 * @param a 线段的一个端点。
 * @param b 线段的另一个端点。
 * @return 如果 (a, b) 是 `polygonVertices` 中的一条边，则返回 true。
 * @note 用于在三角剖分绘图时，区分外边界和内部对角线。
 */
bool DrawingWidget::isPolygonEdge(const QPointF &a, const QPointF &b)
{
    for (int i = 0; i < polygonVertices.size(); ++i) {
        //开始检查列表上的每一条边
        QPointF p1 = polygonVertices[i];//从列表上取出一条“标准边”，由 p1 和 p2 组成
        QPointF p2 = polygonVertices[(i + 1) % polygonVertices.size()];

        //进行关键比对
        if ((a == p1 && b == p2) || (a == p2 && b == p1)) {
            return true; // 说明这条边是外边界
        }
    }
    return false;
}

/**
 * @brief 使用“面积法”判断一个点是否在三角形内部或边界上
 *
 * @details 此方法基于一个几何原理：如果一个点 P 在三角形 ABC 内部，
 * 那么由 P 和三角形三个顶点构成的三个子三角形（PAB, PBC, PCA）的面积之和，
 * 必然精确等于主三角形 ABC 的面积。如果点在外部，则子三角形面积之和会更大。
 *
 * @param pt 要测试的点
 * @return bool 如果点在三角形内或边界上，返回 true；否则返回 false
 *
 * @note 此函数通过计算叉积来得到面积的两倍，避免了除法，且全程使用绝对值，
 * 不受顶点顺序影响。最后的比较使用了浮点数容差(1e-10)来避免精度问题。
 */
bool Triangle::contains(const QPointF &pt) const {
    double totalArea = std::abs((p2.x() - p1.x()) * (p3.y() - p1.y()) -
                                (p2.y() - p1.y()) * (p3.x() - p1.x()));

    double area1 = std::abs((p1.x() - pt.x()) * (p2.y() - pt.y()) -
                            (p1.y() - pt.y()) * (p2.x() - pt.x()));
    double area2 = std::abs((p2.x() - pt.x()) * (p3.y() - pt.y()) -
                            (p2.y() - pt.y()) * (p3.x() - pt.x()));
    double area3 = std::abs((p3.x() - pt.x()) * (p1.y() - pt.y()) -
                            (p3.y() - pt.y()) * (p1.x() - pt.x()));

    return std::abs(area1 + area2 + area3 - totalArea) < 1e-10;
}

/**
 * @brief 检查一个多边形是否为“简单多边形”
 *
 * @details “简单多边形”指其任意两条不相邻的边都不会相交。此函数通过暴力法
 * 遍历多边形的所有不相邻边对，并调用 `segmentsIntersectStrictly`
 * 来检查它们是否严格相交。同时，它也会检查是否存在零长度的退化边。
 *
 * @param poly 以 QVector<QPointF> 形式存储的多边形顶点列表
 * @return bool 如果多边形是简单的（没有自相交），则返回 true；否则返回 false
 *
 * @note 这是执行三角剖分等高级算法前一个至关重要的合法性检查。
 * @complexity O(n^2)，其中 n 是多边形的顶点数。
 */
bool DrawingWidget::isSimplePolygon(const QVector<QPointF> &poly)
{
    int n = poly.size();
    if (n < 3) return true; // 少于3个顶点，无法形成多边形，自然不自相交
    if (n == 3) return true; // 3个顶点总是简单多边形

    for (int i = 0; i < n; ++i) {
        // 当前边 (p1, p2)
        QPointF p1 = poly[i];
        QPointF p2 = poly[(i + 1) % n];

        // 检查零长度边（退化边）。简单多边形不允许顶点重合或边长为零。
        if (p1 == p2) {
            // QMessageBox::warning(this, "警告", QString("多边形存在零长度边：点 P%1 和 P%2 重合").arg(i+1).arg((i+1)%n + 1));
            return false; // 存在零长度边，视为不合法
        }

        for (int j = 0; j < n; ++j) {
            // 另一条边 (q1, q2)
            QPointF q1 = poly[j];
            QPointF q2 = poly[(j + 1) % n];

            // 检查零长度边
            if (q1 == q2) {
                // QMessageBox::warning(this, "警告", QString("多边形存在零长度边：点 P%1 和 P%2 重合").arg(j+1).arg((j+1)%n + 1));
                return false; // 存在零长度边，视为不合法
            }

            // 关键的排除条件：
            // 1. i == j: 两条边是同一条边 (p1p2 == q1q2)
            // 2. j == (i + 1) % n: 两条边是相邻边 (p1p2 和 p2p3)
            // 3. i == (j + 1) % n: 两条边是相邻边 (p1p2 和 p_last_p1)
            if (i == j ||
                j == (i + 1) % n ||
                i == (j + 1) % n)
            {
                continue; // 跳过这些情况，因为它们是合法连接，不构成自相交
            }

            // 额外排除：如果两条边共享一个端点，不视为自相交。
            // 比如 P1-P2 和 P3-P1，它们在 P1 处接触。
            // 即使它们不相邻，但在简单多边形定义中，这种接触不算自相交。
            // segmentsIntersectStrictly 已经排除了端点相交，所以这里不再需要额外判断。

            // 调用严格相交判断
            if (segmentsIntersect(p1, p2, q1, q2)) {
                return false; // 发现严格内部交叉，多边形自相交
            }
        }
    }
    return true; // 没有发现自相交
}

/**
 * @brief 判断一个点是否精确地位于一条线段之上
 *
 * @details 此函数采用两步检查法：
 * 1. **包围盒检查**：快速判断点的坐标是否在线段两个端点构成的矩形范围内。
 * 2. **共线性检查**：通过计算三点叉积是否为零，来精确判断点是否在线段所在的直线上。
 * 只有同时满足这两个条件，点才算在线段上。
 *
 * @param a 线段的起点
 * @param b 线段的终点
 * @param c 要测试的点
 * @return bool 如果点 c 在线段 ab 上，返回 true；否则返回 false
 */
bool DrawingWidget::onSegment(const QPointF &a, const QPointF &b, const QPointF &c) {
    // 检查c是否在ab的包围盒内
    if (c.x() < std::min(a.x(), b.x()) || c.x() > std::max(a.x(), b.x()) ||
        c.y() < std::min(a.y(), b.y()) || c.y() > std::max(a.y(), b.y())) {
        return false;
    }
    // 检查三点是否共线
    return qAbs(crossProduct(a, b, c)) < 1e-10;  // 使用浮点数精度
}

/**
 * @brief 判断两条线段 p1p2 和 q1q2 是否相交（包括端点接触和共线重叠）
 *
 * @details 这是一个标准的线段相交检测算法，分为两部分：
 * 1. **跨立实验**：通过四次叉积判断，检查两条线段的端点是否分别位于对方所在直线的两侧。
 * 这能处理绝大多数“X”型的交叉情况。
 * 2. **共线检查**：处理特殊情况，即当某条线段的一个端点恰好落在另一条线段上时，
 * 也判定为相交。这需要借助 onSegment() 函数。
 *
 * @param p1 线段1的起点
 * @param p2 线段1的终点
 * @param q1 线段2的起点
 * @param q2 线段2的终点
 * @return bool 如果两条线段有任何形式的接触或交叉，返回 true；否则返回 false
 */
bool DrawingWidget::segmentsIntersect(QPointF p1, QPointF p2, QPointF q1, QPointF q2)
{
    // 定义叉积 lambda，确保使用 long long 避免溢出
    auto cross = [](const QPointF&a, const QPointF &b, const QPointF &c) {
        return (long long)(b.x() - a.x()) * (c.y() - a.y()) - (long long)(b.y() - a.y()) * (c.x() - a.x());
    };

    long long o1 = cross(p1, p2, q1);
    long long o2 = cross(p1, p2, q2);
    long long o3 = cross(q1, q2, p1);
    long long o4 = cross(q1, q2, p2);

    // 1. 一般情况：两条线段严格相交（即，每条线段的两个端点在另一条线段的两侧）
    // o1和o2符号不同，且o3和o4符号不同
    if ((o1 > 0 && o2 < 0 || o1 < 0 && o2 > 0) &&
        (o3 > 0 && o4 < 0 || o3 < 0 && o4 > 0)) {
        return true;
    }

    // 2. 处理特殊情况：共线且有重叠（但不是严格内部相交的情况，例如端点重合）
    // 对于简单多边形判断，我们通常不认为共线或端点接触是“自相交”
    // 所以，这里我们只判断严格内部交叉，如果需要处理共线重叠，需要更复杂的逻辑
    // 但对于普通简单多边形判断，这个严格判断已经足够。
    // 如果一条线段的端点在另一条线段的内部，这也算严格相交。
    // 以下是补充处理共线时端点在另一条线段内部的情况（此时叉积为0）
    /*“严格相交”通常有更精确的定义，它指的是两条线段在各自的内部发生了交叉（像一个'X'），而不包括仅仅在端点处发生接触的情况*/
    if (o1 == 0 && onSegment(p1, p2, q1) && !(q1 == p1 || q1 == p2)) return true; // q1在p1p2上且不是p1或p2
    if (o2 == 0 && onSegment(p1, p2, q2) && !(q2 == p1 || q2 == p2)) return true; // q2在p1p2上且不是p1或p2
    if (o3 == 0 && onSegment(q1, q2, p1) && !(p1 == q1 || p1 == q2)) return true; // p1在q1q2上且不是q1或q2
    if (o4 == 0 && onSegment(q1, q2, p2) && !(p2 == q1 || p2 == q2)) return true; // p2在q1q2上且不是q1或q2

    return false; // 其他情况（包括不相交、只在端点处接触、共线但无重叠、共线且端点重叠）
}

/**
 * @brief 使用鞋带公式(Shoelace Formula)计算多边形的有向面积的两倍
 *
 * @details 遍历多边形的所有边，累加每条边与其下一个顶点构成的叉积。
 * 最终结果的符号可以用来判断多边形顶点的环绕方向。
 *
 * @param pts 多边形的顶点列表
 * @return double
 * - > 0: 在Qt坐标系下，表示顶点为顺时针环绕。
 * - < 0: 在Qt坐标系下，表示顶点为逆时针环绕。
 * - = 0: 多边形退化为线段或面积为零。
 * @note 此函数是确保耳切法等算法输入方向一致性的关键。
 */
double DrawingWidget::computeAreaSign(const QVector<QPointF> &pts)
{
    double sum = 0.0;
    for (int i = 0; i < pts.size(); ++i) {
        const QPointF &p1 = pts[i];
        const QPointF &p2 = pts[(i + 1) % pts.size()];
        sum += (p1.x() * p2.y() - p2.x() * p1.y());
    }
    return sum;
}
