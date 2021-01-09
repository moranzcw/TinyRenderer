#include <vector>
#include <iostream>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model = NULL;
float *shadowbuffer = NULL;

const int width = 1600;
const int height = 1600;

Vec3f light_dir(1, 1, 1);
Vec3f eye(1, 1, 4);
Vec3f center(0, 0, 0);
Vec3f up(0, 1, 0);

struct GouraudShader : public IShader
{
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    Vec3f varying_intensity;      // 顶点着色器写入，片段着色器读取

    // iface为面的序号，nthvert为顶点序号
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 计算光照强度
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
        return gl_Vertex;
    }

    // bar为三角形重心坐标
    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算强度插值，并着色
        float intensity = varying_intensity * bar;
        color = TGAColor(255, 255, 255) * intensity;
        return false;
    }
};

struct StylizedGouraudShader : public IShader
{
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    Vec3f varying_intensity;      // 顶点着色器写入，片段着色器读取

    // iface为面的序号，nthvert为顶点序号
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 计算光照强度
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
        return gl_Vertex;
    }

    // bar为三角形重心坐标
    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算强度插值，并着色
        float intensity = varying_intensity * bar;
        if (intensity > .85)
            intensity = 1;
        else if (intensity > .60)
            intensity = .80;
        else if (intensity > .45)
            intensity = .60;
        else if (intensity > .30)
            intensity = .45;
        else if (intensity > .15)
            intensity = .30;
        else
            intensity = 0;
        color = TGAColor(255, 155, 0) * intensity;
        return false;
    }
};

struct TextureGouraudShader : public IShader
{
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    Vec3f varying_intensity;      // 顶点着色器写入，片段着色器读取
    mat<2, 3, float> varying_uv;  // uv坐标，对应三个顶点

    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 从obj文件读取uv坐标
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        // 计算光照强度
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算强度插值，采样纹理，并着色
        float intensity = varying_intensity * bar;
        Vec2f uv = varying_uv * bar;
        color = model->diffuse(uv) * intensity;
        return false;
    }
};

struct NormalMapShader : public IShader
{
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    mat<2, 3, float> varying_uv;  // uv坐标，对应三个顶点
    mat<4, 4, float> uniform_M;   //  Projection*ModelView
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 从obj文件读取uv坐标
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算uv坐标插值
        Vec2f uv = varying_uv * bar;
        // 将法线转换到投影空间
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        // 将光源方向转换到投影空间
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        // 着色
        float intensity = std::max(0.f, n * l);
        color = model->diffuse(uv) * intensity;
        return false;
    }
};

struct PhongShader : public IShader
{
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    mat<2, 3, float> varying_uv;  // uv坐标，对应三个顶点
    mat<4, 4, float> uniform_M;   //  Projection*ModelView
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 从obj文件读取uv坐标
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算uv坐标插值
        Vec2f uv = varying_uv * bar;
        // 将法线转换到投影空间
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        // 将光源方向转换到投影空间
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        // 计算反射光线
        Vec3f r = (n * (n * l * 2.f) - l).normalize();
        // 计算高光强度系数
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        // 计算漫反射强度系数
        float diff = std::max(0.f, n * l);
        // 读取漫反射纹理颜色
        TGAColor c = model->diffuse(uv);
        // 着色
        color = c;
        for (int i = 0; i < 3; i++)
            color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);
        return false;
    }
};

struct TangentNormalMapShader : public IShader
{
    mat<2, 3, float> varying_uv;  // uv坐标，对应三个顶点
    mat<3, 3, float> varying_tri; // 变换后的顶点坐标
    mat<3, 3, float> varying_nrm; // 变换后的顶点法线
    mat<3, 3, float> ndc_tri;     // triangle in normalized device coordinates
    mat<4, 4, float> uniform_M;   //  Projection*ModelView
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()

    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 从.obj文件读取顶点坐标，并转换到裁剪空间
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        // 从obj文件读取uv坐标
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        // 从.obj文件读取顶点法线，并转换到裁剪空间
        varying_nrm.set_col(nthvert, proj<3>(uniform_MIT * embed<4>(model->normal(iface, nthvert), 0.f)));
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        // 为当前像素计算法线插值
        Vec3f bn = (varying_nrm * bar).normalize();
        // 为当前像素计算uv坐标插值
        Vec2f uv = varying_uv * bar;

        mat<3, 3, float> A;
        A[0] = ndc_tri.col(1) - ndc_tri.col(0);
        A[1] = ndc_tri.col(2) - ndc_tri.col(0);
        A[2] = bn;

        mat<3, 3, float> AI = A.invert();

        Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
        Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

        mat<3, 3, float> B;
        B.set_col(0, i.normalize());
        B.set_col(1, j.normalize());
        B.set_col(2, bn);

        Vec3f n = (B * model->tangent_normal(uv)).normalize();

        // 着色
        float diff = std::max(0.f, n * light_dir);
        color = model->diffuse(uv) * diff;

        return false;
    }
};

struct ShadowShader : public IShader
{
    mat<4, 4, float> uniform_M;       //  Projection*ModelView
    mat<4, 4, float> uniform_MIT;     // (Projection*ModelView).invert_transpose()
    mat<4, 4, float> uniform_Mshadow; // transform framebuffer screen coordinates to shadowbuffer screen coordinates
    mat<2, 3, float> varying_uv;      // triangle uv coordinates, written by the vertex shader, read by the fragment shader
    mat<3, 3, float> varying_tri;     // triangle coordinates before Viewport transform, written by VS, read by FS

    ShadowShader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        Vec4f sb_p = uniform_Mshadow * embed<4>(varying_tri * bar); // corresponding point in the shadow buffer
        sb_p = sb_p / sb_p[3];
        int idx = int(sb_p[0]) + int(sb_p[1]) * width;               // index in the shadowbuffer array
        float shadow = .3 + .7 * (shadowbuffer[idx] < sb_p[2] + 2.); // magic coeff to avoid z-fighting
        // 插值uv坐标
        Vec2f uv = varying_uv * bar;
        // 将法线转换到投影空间
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        // 将光源方向转换到投影空间
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        // 计算反射光线
        Vec3f r = (n * (n * l * 2.f) - l).normalize();
        // 计算高光强度系数
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        // 计算漫反射强度系数
        float diff = std::max(0.f, n * l);
        // 读取漫反射纹理
        TGAColor c = model->diffuse(uv);
        // 着色
        for (int i = 0; i < 3; i++)
            color[i] = std::min<float>(20 + c[i] * shadow * (1.2 * diff + .6 * spec), 255);
        return false;
    }
};

struct DepthShader : public IShader
{
    mat<3, 3, float> varying_tri;

    DepthShader() : varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));   // read the vertex from .obj file
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex; // transform it to screen coordinates
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color)
    {
        Vec3f p = varying_tri * bar;
        color = TGAColor(255, 255, 255) * (p.z / depth);
        return false;
    }
};

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return 0;
    }

    float *zbuffer = new float[width * height];
    for (int i = width * height; i--;)
    {
        zbuffer[i] = -std::numeric_limits<float>::max();
    }
    shadowbuffer = new float[width * height];

    light_dir.normalize();

    // 渲染阴影
    {
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(light_dir, center, up);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(0);

        DepthShader depthshader;

        for (int i = 1; i < argc; i++)
        {
            model = new Model(argv[i]);

            Vec4f screen_coords[3];
            for (int i = 0; i < model->nfaces(); i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    screen_coords[j] = depthshader.vertex(i, j);
                }
                triangle(screen_coords, depthshader, depth, shadowbuffer);
            }
            delete model;
        }
        depth.flip_vertically(); // to place the origin in the bottom left corner of the image
        depth.write_tga_file("depth.tga");
    }

    Matrix M = Viewport * Projection * ModelView;

    // 渲染图像
    {
        TGAImage image(width, height, TGAImage::RGB);

        lookat(eye, center, up);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(-1.f / (eye - center).norm());

        // GouraudShader shader;

        // StylizedGouraudShader shader;

        // TextureGouraudShader shader;

        // NormalMapShader shader;
        // shader.uniform_M   =  Projection*ModelView;
        // shader.uniform_MIT = (Projection*ModelView).invert_transpose();

        // PhongShader shader;
        // shader.uniform_M = Projection * ModelView;
        // shader.uniform_MIT = (Projection * ModelView).invert_transpose();

        // TangentNormalMapShader shader;
        // shader.uniform_M   =  Projection*ModelView;
        // shader.uniform_MIT = (Projection*ModelView).invert_transpose();

        ShadowShader shader(Projection * ModelView, (Projection * ModelView).invert_transpose(), M * (Viewport * Projection * ModelView).invert());

        for (int i = 1; i < argc; i++)
        {
            model = new Model(argv[i]);

            Vec4f screen_coords[3];
            for (int i = 0; i < model->nfaces(); i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    screen_coords[j] = shader.vertex(i, j);
                }
                triangle(screen_coords, shader, image, zbuffer);
            }
            delete model;
        }

        image.flip_vertically(); // 上下翻转，让原点在左下角
        image.write_tga_file("output.tga");
    }

    delete[] zbuffer;
    delete[] shadowbuffer;
    return 0;
}
