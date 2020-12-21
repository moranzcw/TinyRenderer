#include <vector>
#include <iostream>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model     = NULL;
const int width  = 1600;
const int height = 1600;

Vec3f light_dir(1,1,1);
Vec3f       eye(0,1,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

struct GouraudShader : public IShader {
    Vec3f varying_intensity; // 顶点着色器写入，片段着色器读取

    // iface为面的序号，nthvert为顶点序号
    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // 从.obj文件读取顶点
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;     // 转换到屏幕空间坐标
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir); // 计算光照强度
        return gl_Vertex;
    }

    // bar为三角形重心坐标
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;   // 为当前像素计算强度插值
        color = TGAColor(255, 255, 255)*intensity; // 着色
        return false;                              // 目前不丢弃该像素
    }
};

int main(int argc, char** argv) {
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }

    lookat(eye, center, up);
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye-center).norm());
    light_dir.normalize();

    TGAImage image  (width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    GouraudShader shader;
    for (int i=0; i<model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j=0; j<3; j++) {
            screen_coords[j] = shader.vertex(i, j);
        }
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.  flip_vertically(); // 上下翻转，让原点在左下角
    zbuffer.flip_vertically();
    image.  write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}
