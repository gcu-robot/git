#include "buff_detect.h"

/**
 * @brief 图像预处理
 * 
 * @param src_img -传入原图像
 * @param enemy_color -传入敌方颜色
 */
void Max_Buff::pretreat(Mat src_img, int enemy_color)
{
    //保存原图像
    this->frame = src_img;
    //转灰度图
    Mat gray_img;
    blur(src_img, src_img, Size(7, 7));
    cvtColor(src_img, gray_img, COLOR_BGR2GRAY);
    //分离通道
    vector<Mat> _split;
    split(src_img, _split);
    //判断颜色
    Mat bin_img_color, bin_img_gray;
    namedWindow("src_img", WINDOW_AUTOSIZE);
    if (enemy_color == 0)
    {
        subtract(_split[0], _split[2], bin_img_color); // b - r
#if IS_PARAM_ADJUSTMENT == 1
        createTrackbar("GRAY_TH_BLUE:", "src_img", &this->blue_armor_gray_th, 255, NULL);
        createTrackbar("COLOR_TH_BLUE:", "src_img", &this->blue_armor_color_th, 255, NULL);
        threshold(gray_img, bin_img_gray, this->blue_armor_gray_th, 255, THRESH_BINARY);
        threshold(bin_img_color, bin_img_color, this->blue_armor_color_th, 255, THRESH_BINARY);
#else
        threshold(gray_img, bin_img_gray, blue_armor_gray_th, 255, THRESH_BINARY);
        threshold(bin_img_color, bin_img_color, blue_armor_color_th, 255, THRESH_BINARY);
#endif
    }
    else if (enemy_color == 1)
    {
        subtract(_split[2], _split[0], bin_img_color); // r - b
#if IS_PARAM_ADJUSTMENT == 1
        createTrackbar("GRAY_TH_RED:", "src_img", &this->red_armor_gray_th, 255);
        createTrackbar("COLOR_TH_RED:", "src_img", &this->red_armor_color_th, 255);
        threshold(gray_img, bin_img_gray, this->red_armor_gray_th, 255, THRESH_BINARY);
        threshold(bin_img_color, bin_img_color, this->red_armor_color_th, 255, THRESH_BINARY);
#else
        threshold(gray_img, bin_img_gray, red_armor_gray_th, 255, THRESH_BINARY);
        threshold(bin_img_color, bin_img_color, red_armor_color_th, 255, THRESH_BINARY);
#endif
    }
    Mat element = getStructuringElement(MORPH_ELLIPSE, cv::Size(3, 3));
    Mat dilate_element = getStructuringElement(MORPH_ELLIPSE, cv::Size(9, 9));
#if SHOW_BIN_IMG == 1
    imshow("gray_img", bin_img_gray);
    imshow("mask", bin_img_color);
#endif
    // medianBlur(bin_img_color, bin_img_color, 9);
    morphologyEx(bin_img_color, bin_img_color, MORPH_OPEN, element);
    dilate(bin_img_gray, bin_img_gray, dilate_element);
    bitwise_and(bin_img_color, bin_img_gray, bin_img_color);
#if SHOW_BIN_IMG == 1
    imshow("src_img", bin_img_color);
#endif
    //保存处理后的图片
    this->mask = bin_img_color;
    this->gray_img = bin_img_gray;
}
/**
 * @brief 寻找R和大神符击打中心点
 * 
 * @return true 
 * @return false 
 */
void Max_Buff::Looking_for_center()
{
    vector<vector<Point>> contours;
    vector<vector<Point>> contours_external;
    vector<Vec4i> hierarchy;
    findContours(this->mask, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_NONE);
    // findContours(this->mask, contours_external, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_NONE);
    RotatedRect minRect;
    int num = 0;
    vector<vector<Point>> contours_poly(contours.size());
    for (size_t i = 0; i < contours.size(); i++)
    {
        if (contours[i].size() < 6 || contourArea(contours[i]) < 100)
        {
            continue;
        }
        // drawContours(frame, contours, i, Scalar(0, 255, 255), 3, 8);
        minRect = minAreaRect(contours[i]);
        float w_h_ratio = minRect.size.width / minRect.size.height;
        float Area_ratio = contourArea(contours[i]) / (minRect.size.width * minRect.size.height);
        if (w_h_ratio < 1.1 && w_h_ratio > 0.9 && Area_ratio > 0.76)
        {
            R_center = minRect.center;
            R_success = true;
            circle(frame, R_center, 5, Scalar(255, 0, 0), -1);
        }
    }
    for (size_t i = 0; i < contours.size(); i++)
    {
        if (contours[i].size() < 6 || hierarchy[i][2] > 0)
        {
            continue;
        }
        // drawContours(frame, contours, i, Scalar(0, 255, 255), 3, 8);
        minRect = minAreaRect(contours[i]);
        //通过大轮廓 面积比和长宽比判断击打位置
        if ((minRect.size.width * minRect.size.height > 300) && (contourArea(contours[i]) / (minRect.size.width * minRect.size.height) > 0.8) && (((minRect.size.width / minRect.size.height) > 1.2 && (minRect.size.width / minRect.size.height) < 2.1) || ((minRect.size.height / minRect.size.width) > 1.2 && (minRect.size.height / minRect.size.width) < 2.1)))
        {
            if (minRect.angle > 90.0f)
                minRect.angle = minRect.angle - 180.0f;
            Point2f vertex[4];
            minRect.points(vertex);
            for (int i = 0; i < 4; i++)
            {
                line(frame, vertex[i], vertex[(i + 1) % 4], Scalar(255, 100, 200), 2, CV_AA);
            }
            max_buff_rects.push_back(minRect); //保存外接矩形
            Mat src = max_buff_roi(num);
            // imshow("s", src);
            if (average_color(src) < 100 && R_success) //20红色 50蓝色
            {
                hit_subscript = num;
                choice_success = true;
            }
            else if (average_color(src) < 15 && !R_success)
            {
                hit_subscript = num;
                choice_success = true;
            }
            num++;
        }
    }
    if (R_success)
    {
        if (choice_success)
        {
            radius = sqrt((R_center.x - max_buff_rects[0].center.x) * (R_center.x - max_buff_rects[0].center.x) + (R_center.y - max_buff_rects[0].center.y) * (R_center.y - max_buff_rects[0].center.y));
            circle(frame, R_center, radius, Scalar(255, 255, 0), 3);
            circle(frame, max_buff_rects[hit_subscript].center, 7, Scalar(0, 0, 255), -1);
            line(frame, R_center, max_buff_rects[hit_subscript].center, Scalar(0, 0, 255), 3);
            Calculating_coordinates(hit_subscript);
            choice_success = false;
            R_success = false;
        }
    }
    else
    {
        if (choice_success)
        {
            circle(frame, max_buff_rects[hit_subscript].center, 7, Scalar(0, 0, 255), -1);
            choice_success = false;
            R_success = false;
        }
    }
}

Mat Max_Buff::max_buff_roi(int i)
{
    int _w = MAX(max_buff_rects[i].size.width, max_buff_rects[i].size.height);
    if (R_success)
    {
        Point center = Point((max_buff_rects[i].center.x + R_center.x) / 2, (max_buff_rects[i].center.y + R_center.y) / 2);
        // int radius = sqrt((R_center.x - max_buff_rects[i].center.x) * (R_center.x - max_buff_rects[i].center.x) + (R_center.y - max_buff_rects[i].center.y) * (R_center.y - max_buff_rects[i].center.y));
        //计算参数方程
        RotatedRect rect = RotatedRect(center, Size(_w * 2, _w * 2), max_buff_rects[i].angle);
        Point tl = Point(rect.center.x - (rect.size.width / 2), rect.center.y - (rect.size.height / 2));
        Point bl = Point(rect.center.x + (rect.size.width / 2), rect.center.y + (rect.size.height / 2));
        if (tl.x < 0)
        {
            tl.x = 0;
        }
        if (bl.x > CAMERA_RESOLUTION_COLS)
        {
            bl.x = CAMERA_RESOLUTION_COLS;
        }
        if (tl.y < 0)
        {
            tl.y = 0;
        }
        if (bl.y > CAMERA_RESOLUTION_ROWS)
        {
            bl.y = CAMERA_RESOLUTION_ROWS;
        }
        Rect rects = Rect(tl, bl);
        Mat src = frame(rects);
        return src;
    }
    else
    {
        RotatedRect rect = RotatedRect(max_buff_rects[i].center, Size(_w * 4, _w * 4), max_buff_rects[i].angle);
        Point tl = Point(rect.center.x - (rect.size.width / 2), rect.center.y - (rect.size.height / 2));
        Point bl = Point(rect.center.x + (rect.size.width / 2), rect.center.y + (rect.size.height / 2));
        if (tl.x < 0)
        {
            tl.x = 0;
        }
        if (bl.x > CAMERA_RESOLUTION_COLS)
        {
            bl.x = CAMERA_RESOLUTION_COLS;
        }
        if (tl.y < 0)
        {
            tl.y = 0;
        }
        if (bl.y > CAMERA_RESOLUTION_ROWS)
        {
            bl.y = CAMERA_RESOLUTION_ROWS;
        }
        Rect rects = Rect(tl, bl);
        Mat src = frame(rects);
        return src;
    }
}

/**
 * @brief 计算图像中像素点的平均强度
 * 
 * @param roi 传入需要计算的图像
 * @return int 返回平均强度
 */
int Max_Buff::average_color(Mat roi)
{
    int average_intensity = static_cast<int>(mean(roi).val[0]);
    // cout << average_intensity << endl;
    return average_intensity;
}

/**
 * @brief 计算其他击打位置坐标（参数方程计算有问题）
 * 
 */
void Max_Buff::Calculating_coordinates(int i)
{
    // float radius = sqrt((R_center.x - max_buff_rects[i].center.x) * (R_center.x - max_buff_rects[i].center.x) + (R_center.y - max_buff_rects[i].center.y) * (R_center.y - max_buff_rects[i].center.y));
    //计算参数方程
    float a = (max_buff_rects[i].center.x - R_center.x) / radius;
    float b = (max_buff_rects[i].center.y - R_center.y) / radius;
    angle_cos = acos(a) * ARC_ANGLE;
    angle_sin = asin(b) * ARC_ANGLE;

    if (angle_cos > 180 || angle_cos < -180)
    {
        angle = angle_sin + forecast_angle;
        calculation_position = Point(R_center.x - (radius * cos((angle) / ARC_ANGLE)), R_center.y - (radius * sin((angle) / ARC_ANGLE)));
    }
    else if (angle_sin > 180 || angle_sin < -180)
    {
        angle = angle_cos + forecast_angle;
        calculation_position = Point(R_center.x - (radius * cos((angle) / ARC_ANGLE)), R_center.y - (radius * sin((angle) / ARC_ANGLE)));
    }
    else
    {
        if (abs(angle_cos) == abs(angle_sin) && angle_sin < 0)
        {
            angle = angle_cos + forecast_angle;
            calculation_position = Point(R_center.x + (radius * cos((angle) / ARC_ANGLE)), R_center.y - (radius * sin((angle) / ARC_ANGLE)));
        }
        else if (abs(angle_cos) == abs(angle_sin) && angle_sin > 0)
        {
            angle = angle_cos - forecast_angle;
            calculation_position = Point(R_center.x + (radius * cos((angle) / ARC_ANGLE)), R_center.y + (radius * sin((angle) / ARC_ANGLE)));
        }
        else if (abs(angle_cos) > abs(angle_sin) && angle_sin < 0)
        {
            angle = angle_cos + forecast_angle;
            calculation_position = Point(R_center.x + (radius * cos((angle) / ARC_ANGLE)), R_center.y - (radius * sin((angle) / ARC_ANGLE)));
        }
        else if (abs(angle_cos) > abs(angle_sin) && angle_sin > 0)
        {
            angle = angle_cos - forecast_angle;
            calculation_position = Point(R_center.x + (radius * cos((angle) / ARC_ANGLE)), R_center.y + (radius * sin((angle) / ARC_ANGLE)));
        }
    }
    cout << angle << endl;
    circle(frame, calculation_position, 10, Scalar(0, 200, 255), -1);
}