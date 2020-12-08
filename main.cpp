#include "tgaimage.h"
// #include "model.h"
#include "geometry.h"
#include <algorithm>
#include <iostream>

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green   = TGAColor(0, 255,   0,   255);

const int width  = 200;
const int height = 200;

void line(int x0, int y0, int x1, int y1, TGAImage &image, TGAColor color) { 
    bool steep = false; 
    if (std::abs(x0-x1)<std::abs(y0-y1)) { 
        std::swap(x0, y0); 
        std::swap(x1, y1); 
        steep = true; 
    } 
    if (x0>x1) { 
        std::swap(x0, x1); 
        std::swap(y0, y1); 
    } 
    int dx = x1-x0; 
    int dy = y1-y0; 
    int derror2 = std::abs(dy)*2; 
    int error2 = 0; 
    int y = y0; 
    for (int x=x0; x<=x1; x++) { 
        if (steep) { 
            image.set(y, x, color); 
        } else { 
            image.set(x, y, color); 
        } 
        error2 += derror2; 
        if (error2 > dx) { 
            y += (y1>y0?1:-1); 
            error2 -= dx*2; 
        } 
    } 
}

void line(Vec2i t0, Vec2i t1, TGAImage &image, TGAColor color){
    line(t0.x, t0.y, t1.x, t1.y, image, color);
}

// 画出三角形轮廓
void triangle1(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) { 
    line(t0, t1, image, color); 
    line(t1, t2, image, color); 
    line(t2, t0, image, color); 
}

// 画出三角形轮廓
void triangle2(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) { 
    // 按y坐标从低到高排序
    if (t0.y>t1.y) std::swap(t0, t1); 
    if (t0.y>t2.y) std::swap(t0, t2); 
    if (t1.y>t2.y) std::swap(t1, t2);

    line(t0, t1, image, green); 
    line(t1, t2, image, green); 
    line(t2, t0, image, red); 
}

// 填充三角形
void triangle3(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) { 
    // 按y坐标从低到高排序
    if (t0.y>t1.y) std::swap(t0, t1); 
    if (t0.y>t2.y) std::swap(t0, t2); 
    if (t1.y>t2.y) std::swap(t1, t2); 

    std::cout<<t0.y<<" "<<t1.y<<" "<<t2.y<<std::endl;
    // 画出三角形的下半部分
    int total_height = t2.y-t0.y; 
    for (int y=t0.y; y<=t1.y; y++) { 
        int segment_height = t1.y-t0.y+1; // +1可以让其不为零，但会导致segment_height偏大
        float alpha = (float)(y-t0.y)/total_height; // 可能产生除零错误
        float beta  = (float)(y-t0.y)/segment_height; // 注意不要除零错误
        Vec2i A = t0 + (t2-t0)*alpha; 
        Vec2i B = t0 + (t1-t0)*beta; 
        if (A.x>B.x) std::swap(A, B); 
        for (int j=A.x; j<=B.x; j++) { 
            image.set(j, y, color); // 注意int转化 t0.y+i != A.y 
        } 
    } 
    // 画出三角形的上半部分
    for (int y=t1.y; y<=t2.y; y++) { 
        int segment_height =  t2.y-t1.y+1; // +1可以让其不为零，但会导致segment_height偏大
        float alpha = (float)(y-t0.y)/total_height; // 可能产生除零错误
        float beta  = (float)(y-t1.y)/segment_height; // 注意不要除零错误
        Vec2i A = t0 + (t2-t0)*alpha; 
        Vec2i B = t1 + (t2-t1)*beta; 
        if (A.x>B.x) std::swap(A, B); 
        for (int j=A.x; j<=B.x; j++) { 
            image.set(j, y, color); // 注意int转化 t0.y+i != A.y 
        } 
    } 
}

// 填充三角形
void triangle4(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) { 
    if (t0.y==t1.y && t0.y==t2.y) return; // 忽略三点y坐标相等的三角形，避免后面的total_height为零
    // 按y坐标从低到高排序
    if (t0.y>t1.y) std::swap(t0, t1); 
    if (t0.y>t2.y) std::swap(t0, t2); 
    if (t1.y>t2.y) std::swap(t1, t2); 

    int total_height = t2.y-t0.y; 
    for (int i=0; i<total_height; i++) { 
        bool upper_half = i>t1.y-t0.y || t1.y==t0.y; // 前一条件若t1==t2时，则跳过上半部分，后一条件若t1.y==t0.y时，则跳过下半部分，解决除零错误
        int segment_height = upper_half ? t2.y-t1.y : t1.y-t0.y; 
        float alpha = (float)i/total_height; 
        float beta  = (float)(i-(upper_half ? t1.y-t0.y : 0))/segment_height; // 在上述条件下，这里不会有除零错误 
        Vec2i A =               t0 + (t2-t0)*alpha; 
        Vec2i B = upper_half ? t1 + (t2-t1)*beta : t0 + (t1-t0)*beta; 
        if (A.x>B.x) std::swap(A, B); 
        for (int j=A.x; j<=B.x; j++) { 
            image.set(j, t0.y+i, color); // 注意int转化 t0.y+i != A.y 
        } 
    } 
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);

    Vec2i t0[3] = {Vec2i(10, 70),   Vec2i(50, 160),  Vec2i(70, 80)}; 
    Vec2i t1[3] = {Vec2i(180, 50),  Vec2i(150, 1),   Vec2i(70, 180)}; 
    Vec2i t2[3] = {Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180)}; 
    triangle4(t0[0], t0[1], t0[2], image, red); 
    triangle4(t1[0], t1[1], t1[2], image, white); 
    triangle4(t2[0], t2[1], t2[2], image, green);

    image.flip_vertically(); // 上下翻转，让坐标原点在左下角
    image.write_tga_file("output.tga");

    return 0;
}