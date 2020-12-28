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
    Vec3f varying_intensity;    // 顶点着色器写入，片段着色器读取

    // iface为面的序号，nthvert为顶点序号
    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));    // 从.obj文件读取顶点坐标
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;        // 转换坐标到屏幕空间
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir);    // 计算光照强度
        return gl_Vertex;
    }

    // bar为三角形重心坐标
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;    // 为当前像素计算强度插值
        color = TGAColor(255, 255, 255)*intensity;  // 着色
        return false;                               // 不丢弃该像素
    }
};

struct StylizedGouraudShader : public IShader {
    Vec3f varying_intensity;    // 顶点着色器写入，片段着色器读取

    // iface为面的序号，nthvert为顶点序号
    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));    // 从.obj文件读取顶点
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;        // 转换坐标到屏幕空间
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir);    // 计算光照强度
        return gl_Vertex;
    }

    // bar为三角形重心坐标
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;    // 为当前像素计算强度插值
        if (intensity>.85) intensity = 1;
        else if (intensity>.60) intensity = .80;
        else if (intensity>.45) intensity = .60;
        else if (intensity>.30) intensity = .45;
        else if (intensity>.15) intensity = .30;
        else intensity = 0;
        color = TGAColor(255, 155, 0)*intensity;    // 着色
        return false;                               // 不丢弃该像素
    }
};

struct TextureGouraudShader : public IShader {
    Vec3f          varying_intensity;   // 顶点着色器写入，片段着色器读取
    mat<2,3,float> varying_uv;          // uv坐标，对应三个顶点

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));    // 从obj文件读取顶点坐标
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;        // 转换坐标到屏幕空间
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));     // 从obj文件读取uv坐标
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert)*light_dir);    // 计算光照强度
        return gl_Vertex;
    }
    
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;    // 为当前像素计算强度插值
        Vec2f uv = varying_uv*bar;                  // 为当前像素uv坐标计算插值
        color = model->diffuse(uv)*intensity;       // 着色
        return false;                               // 不丢弃该像素
    }
};

struct NormalMapShader : public IShader {
    mat<2,3,float> varying_uv;  // uv坐标，对应三个顶点
    mat<4,4,float> uniform_M;   //  Projection*ModelView
    mat<4,4,float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // 从obj文件读取顶点坐标
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;     // 转换坐标到屏幕空间
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));  // 从obj文件读取uv坐标
        return gl_Vertex; 
   }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv*bar;                  // 为当前像素uv坐标计算插值
        Vec3f n = proj<3>(uniform_MIT*embed<4>(model->normal(uv))).normalize(); // 将法线转换到屏投影空间
        Vec3f l = proj<3>(uniform_M  *embed<4>(light_dir        )).normalize(); // 将光源方向转换到投影空间
        float intensity = std::max(0.f, n*l);
        color = model->diffuse(uv)*intensity;       // 着色
        return false;                               // 不丢弃该像素
    }
};

struct PhongShader : public IShader {
    mat<2,3,float> varying_uv;  // uv坐标，对应三个顶点
    mat<4,4,float> uniform_M;   //  Projection*ModelView
    mat<4,4,float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));    // 从obj文件读取顶点坐标
        gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;        // 转换坐标到屏幕空间
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));     // 从obj文件读取uv坐标
        return gl_Vertex; 
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv*bar;                  // 为当前像素uv坐标计算插值
        Vec3f n = proj<3>(uniform_MIT*embed<4>(model->normal(uv))).normalize(); // 将法线转换到屏投影空间
        Vec3f l = proj<3>(uniform_M  *embed<4>(light_dir        )).normalize(); // 将光源方向转换到投影空间
        Vec3f r = (n*(n*l*2.f) - l).normalize();    // 计算反射光线
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv)); // 计算高光强度系数
        float diff = std::max(0.f, n*l);    // 计算漫反射强度系数
        TGAColor c = model->diffuse(uv);    // 读取漫反射颜色
        color = c;
        for (int i=0; i<3; i++)
            color[i] = std::min<float>(5 + c[i]*(diff + .6*spec), 255); // 着色，5为环境光
        return false;   // 不丢弃该像素
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

    // GouraudShader shader;
    // StylizedGouraudShader shader;
    // TextureGouraudShader shader;
    // NormalMapShader shader;
    // shader.uniform_M   =  Projection*ModelView;
    // shader.uniform_MIT = (Projection*ModelView).invert_transpose();
    PhongShader shader;
    shader.uniform_M   =  Projection*ModelView;
    shader.uniform_MIT = (Projection*ModelView).invert_transpose();
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
