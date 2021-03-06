//
// structured - Tools for the Generation and Visualization of Large-scale
// Three-dimensional Reconstructions from Image Data. This software includes
// source code from other projects, which is subject to different licensing,
// see COPYING for details. If this project is used for research see COPYING
// for making the appropriate citations.
// Copyright (C) 2013 Matthew Johnson-Roberson <mattkjr@gmail.com>
//
// This file is part of structured.
//
// structured is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// structured is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with structured.  If not, see <http://www.gnu.org/licenses/>.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
// this define is mandatory to avoid the conflicts due to the silly definition of
// min and max macros in windows.h (included by glut...)
#define NOMINMAX
#ifdef USEGL

#include <GL/glew.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <wrap/gl/space.h>
#include <wrap/gui/trackball.h>

#endif


#include <iostream>

#include <osgDB/FileNameUtils>
#include <wrap/callback.h>
#include <vcg/math/base.h>
/*#include <vcg/simplex/vertex/with/vcvn.h>
#include <vcg/simplex/vertex/with/vcvn.h>
#include <vcg/simplex/face/with/fn.h>
#include <vcg/space/index/grid_static_ptr.h>
#include <vcg/complex/trimesh/base.h>
#include<wrap/io_trimesh/export_ply.h>
#include<wrap/io_trimesh/import_ply.h>
#include<vcg/complex/trimesh/update/normal.h>
#include<vcg/complex/trimesh/update/bounding.h>
#include<vcg/complex/trimesh/update/color.h>
*/
// stuff to define the mesh
#include <vcg/simplex/vertex/base.h>
#include <vcg/simplex/face/base.h>
#include <vcg/simplex/edge/base.h>


#include <vcg/math/quadric.h>
#include <vcg/complex/algorithms/clean.h>

// io
#include <wrap/io_trimesh/import.h>
#include <wrap/io_trimesh/export_ply.h>
#include <wrap/io_trimesh/export_obj.h>
// *include the algorithms for updating: */
#include <vcg/complex/algorithms/update/topology.h>	/* topology */
#include <vcg/complex/algorithms/update/bounding.h>	/* bounding box */
#include <vcg/complex/algorithms/update/normal.h>		/* normal */
#include <time.h>
#include <string>
#include <iostream>
#include <osg/TriangleIndexFunctor>
#include <osg/Timer>
// requires software renderer source
#include "../swrender/renderer/geometry_processor.h"
#include "../swrender/renderer/rasterizer_subdivaffine.h"
#include "../swrender/renderer/span.h"
#include "../swrender/democommon.h"
#include "../swrender/fixedpoint/fixed_func.h"
#include <osg/io_utils>
#include <osg/ComputeBoundsVisitor>
#include "vertexData.h"
#include <osgDB/ReadFile>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>
#include <opencv/cv.h>
#include <osgDB/FileNameUtils>
#include <opencv/highgui.h>
#include "../swrender/mipmap.h"
#include "../swrender/render_utils.h"
#include "../MemUtils.h"
#include "../GLImaging.h"
#include "../swrender/VertexShaders.h"
#include "../swrender/FragmentShaders.h"
#include "../swrender/Raster.h"
#include "../swrender/VipsSampler.h"
#include "GenParam.h"
// the software renderer stuff is located in the namespace "swr" so include
// that here
using namespace swr;
using namespace std;
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#include <vips/vips>
#include <osgDB/FileUtils>
#include <vips/vips.h>
bool removeDups(std::string basename,osg::Vec3Array *verts,osg::DrawElementsUInt * triangles);
static osg::Vec3 debugV;
bool USE_BGR=false;
struct InputVertex {
    vec3x vertex;
};
static REGION *regRange[2];
static REGION *regOutput[2];


static IMAGE *outputImage[2];
static IMAGE *rangeImage[2];
/* Generate function --- just black out the region.*/
static int white_gen( REGION *reg, void *seq, void *a, void *b )
{
    PEL *q = (PEL *) IM_REGION_ADDR( reg, reg->valid.left, reg->valid.top );
    int wd = IM_REGION_SIZEOF_LINE( reg );
    int ls = IM_REGION_LSKIP( reg );
    int y;

    for( y = 0; y < reg->valid.height; y++, q += ls )
        memset( (char *) q, 255, wd );

    return( 0 );
}
osg::Vec3Array * doMeshReorder(std::string basename);


/**
 * im_black:
 * @out: output #IMAGE
 * @x: output width
 * @y: output height
 * @bands: number of output bands
 *
 * Make a black unsigned char image of a specified size.
 *
 * See also: im_make_xy(), im_text(), im_gaussnoise().
 *
 * Returns: 0 on success, -1 on error
 */
int
im_white( IMAGE *out, int x, int y, int bands )
{
    if( x <= 0 || y <= 0 || bands <= 0 ) {
        im_error( "im_white", "%s", "bad parameter"  );
        return( -1 );
    }

    if( im_poutcheck( out ) )
        return( -1 );
    im_initdesc( out,
                 x, y, bands,
                 IM_BBITS_BYTE, IM_BANDFMT_UCHAR, IM_CODING_NONE,
                 bands == 1 ? IM_TYPE_B_W : IM_TYPE_MULTIBAND,
                 1.0, 1.0, 0, 0 );
    if( im_demand_hint( out, IM_ANY, NULL ) )
        return( -1 );

    if( im_generate( out, NULL, white_gen, NULL, NULL, NULL ) )
        return( -1 );

    return( 0 );
}






void write_header(std::ostream& _fout,int total_face_count,bool color){
    _fout <<"ply\n";
    _fout <<"format binary_little_endian 1.0\n";
    //_fout <<"comment PLY exporter written by Paul Adams\n";
    _fout <<"element vertex "<<total_face_count <<std::endl;
    _fout <<"property float x\n";
    _fout <<"property float y\n";
    _fout <<"property float z\n";
    if(color){
        _fout <<"property uchar red\n";
        _fout <<"property uchar green\n";
        _fout <<"property uchar blue\n";
    }
    _fout <<"element face " <<total_face_count/3<<std::endl;
    _fout <<"property list uchar int vertex_indices\n";
    _fout <<"property list uchar float texcoord\n";
    _fout <<"property int texnumber\n";
    _fout <<"property float quality\n";
    _fout <<"end_header\n";
}
void write_all(std::ostream& _fout,osg::DrawElementsUInt *tri,osg::Vec3Array *verts,osg::Vec4Array *colors,const std::vector<int> &imageId,osg::Vec2Array *tcarr,const int mosaic,bool flip,osg::Vec2Array *valid){
    int cnt=0;
    for(int i=0; i< (int)tri->size()-2; i+=3){
        if(valid != NULL && valid->at(i/3).x() == -999.0)
            continue;
        for(int j=0; j<3; j++){
            osg::Vec3 v=verts->at(tri->at(i+j));
            float vf[3];
            vf[0]=v[0];
            vf[1]=v[1];
            vf[2]=v[2];
            _fout.write((char *)vf,3*sizeof(float));
             if(colors != NULL && i+j <(int)colors->size() ){
                unsigned char col[3];
                osg::Vec4 c=colors->at(i+j);
                // cout <<c<<endl;
                col[0]=c[0]*255.0;
                col[1]=c[1]*255.0;
                col[2]=c[2]*255.0;
                _fout.write((char *)col,3*sizeof(unsigned char));

            }
        }

    }
    int iout[3];
    unsigned char c12=4*3;

    unsigned char c3=3;
    unsigned char ctex=3*2;
    float fout[6];
    float cfout[4];
    cfout[0]=cfout[1]=cfout[2]=cfout[3]=0;
    for(int i=0; i<(int) tri->size()-2; i+=3){
        if(valid->at(i/3).x() == -999.0)
            continue;
        _fout.write((char *)&c3,sizeof(char));

        if(flip){
            iout[0]=cnt+0;
            iout[1]=cnt+1;
            iout[2]=cnt+2;
        }else{
            iout[0]=cnt+2;
            iout[1]=cnt+1;
            iout[2]=cnt+0;

        }
        _fout.write((char*)iout,sizeof(int)*3);


        _fout.write((char *)&ctex,sizeof(char));
        for(int j=0; j<3; j++){
            osg::Vec2 tc=tcarr->at(tri->at(i+j));
            fout[(j*2)+0]=tc.x();
            fout[(j*2)+1]=tc.y();
        }
        _fout.write((char*)fout,sizeof(float)*ctex);
        iout[0]= ((i/3) < imageId.size()) ? imageId.at(i/3) : 1;
        _fout.write((char*)iout,sizeof(int));

        fout[0]=(float)mosaic;
        _fout.write((char*)fout,sizeof(float));

        cnt+=3;

    }


}






void readFile(string fname,map<int,imgData> &imageList){

    std::ifstream m_fin(fname.c_str());
    if(!osgDB::fileExists(fname.c_str())||!m_fin.good() ){
        fprintf(stderr,"Can't load %s\n",fname.c_str());
        exit(-1);
    }
    while(!m_fin.eof()){
        imgData cam;
        double low[3], high[3];
        if(m_fin >> cam.id >> cam.filename >> low[0] >> low[1] >> low[2] >> high[0] >> high[1] >> high[2]
                >> cam.m(0,0) >>cam.m(0,1)>>cam.m(0,2) >>cam.m(0,3)
                >> cam.m(1,0) >>cam.m(1,1)>>cam.m(1,2) >>cam.m(1,3)
                >> cam.m(2,0) >>cam.m(2,1)>>cam.m(2,2) >>cam.m(2,3)
                >> cam.m(3,0) >>cam.m(3,1)>>cam.m(3,2) >>cam.m(3,3)){
            cam.bbox.expandBy(low[0],low[1],low[2]);
            cam.bbox.expandBy(high[0],high[1],high[2]);
            imageList[cam.id]=cam;
        }
    }
}


static Vertex tmp_vertices[3];

inline bool process_tri(ply::tri_t &tri,osg::Vec3Array *verts, std::vector<osg::ref_ptr<osg::Vec3Array> >   &texCoord, bool blending)
{
    int pos=tri.pos;


    VertexShaderBlending::triIdx=tri.tri_idx;
    FragmentShaderBlendingMain::triIdx=tri.tri_idx;
    FragmentShaderBlendingDistPass::idx =pos;
    FragmentShaderBlendingDistPass::triIdx=tri.tri_idx;
    if(blending)
        FragmentShaderBlendingMain::idx=pos;
    osg::Vec3 v1=verts->at(tri.idx[0]);
    osg::Vec3 v2=verts->at(tri.idx[1]);
    osg::Vec3 v3=verts->at(tri.idx[2]);
    osg::Vec3 tc1,tc2,tc3;

    tc1=texCoord[pos]->at(tri.idx[0]);
    tc2=texCoord[pos]->at(tri.idx[1]);;

    tc3=texCoord[pos]->at(tri.idx[2]);;



    if(tc1.x() <0 || tc1.y() <0 || tc2.x() <0 || tc2.y() <0 || tc3.x() <0 || tc3.y() <0){
        //printf("Fail %f %f %f %f %f %f\n",tc1.x(),tc1.y(),tc2.x(),tc2.y(),tc3.x(),tc3.y());
        return false;
    }
    tmp_vertices[0].x=v1.x();
    tmp_vertices[0].y=v1.y();
    tmp_vertices[0].tx=tc1.x();
    tmp_vertices[0].ty=1.0-tc1.y();
    tmp_vertices[0].id=tc1.z();

    tmp_vertices[1].x=v2.x();
    tmp_vertices[1].y=v2.y();
    tmp_vertices[1].tx=tc2.x();
    tmp_vertices[1].ty=1.0-tc2.y();
    tmp_vertices[1].id=tc2.z();

    tmp_vertices[2].x=v3.x();
    tmp_vertices[2].y=v3.y();
    tmp_vertices[2].tx=tc3.x();
    tmp_vertices[2].ty=1.0-tc3.y();
    tmp_vertices[2].id=tc3.z();
    return true;
}
typedef struct _SamplerData{
    osg::Vec2 positions[4];
    osg::Vec3 normals[4];
    osg::Vec2 texcoords[4];
}SamplerData;

inline bool process_tri_new(ply::tri_t &tri,osg::Vec3Array *verts,
                            std::vector<osg::ref_ptr<osg::Vec3Array> >   &texCoord,
                            bool blending,
                            int sizeX,
                            int sizeY,
                            VipsSampler &viSamp,
                            SamplerData &samp,
                            osg::Matrix *vp=NULL
                            )
{
    int pos=tri.pos;




    int vtxCount=3;

    for (uint k = 0; k < vtxCount; k++)
    {
        double u=texCoord[pos]->at(tri.idx[k]).x();
        double v=1.0-texCoord[pos]->at(tri.idx[k]).y();

        if(u<0.0 || u >1.0 || v<0.0 ||v>1.0)
            return false;
        osg::Vec3 &vert=verts->at(tri.idx[k]);
        osg::Vec4 v4d(vert.x(),vert.y(),vert.z(),1.0);

        if(vp != NULL){
            v4d=v4d*(*vp);
            v4d.x() /= v4d.w();
            v4d.y() /= v4d.w();
            v4d.z() /= v4d.w();
            v4d.w() /= v4d.w();
            //v4d.y() = 1.0 -v4d.y();
            v4d= v4d*( osg::Matrix::translate(1.0,1.0,1.0)*osg::Matrix::scale(0.5*sizeX,0.5*sizeY,0.5f));
            samp.positions[k].set(v4d.x(),
                                  sizeY-v4d.y());
        }else{
            //Already projected just scale to pixel size
            samp.positions[k].set(verts->at(tri.idx[k]).x()*sizeX,
                                   (1.0- verts->at(tri.idx[k]).y())*sizeY);
        }
        samp.texcoords[k].set(u,v);

    }


  //  viSamp.setCurrentFace(vtxCount, positions, normals);
    viSamp.triIdx=tri.tri_idx;
    viSamp.idx=pos;

    return true;
}

int main(int ac, char *av[]) {
    string path=string(av[0]);
    unsigned int loc=path.rfind("/");

    string basepath= loc == string::npos ? "./" : path.substr(0,loc+1);
    basepath= osgDB::getRealPath (basepath);
    basepath+="/../../";
    osg::Timer_t start;
    osg::ref_ptr<osg::Node> tmpmodel;//= osgDB::readNodeFile(av[1]);
    ply::VertexData vertexData[2];
    osg::ArgumentParser arguments(&ac,av);
    int passesFlatRemap=1;
    for(int i=0; i<passesFlatRemap; i++){
        outputImage[i]=NULL;
        rangeImage[i]=NULL;
    }
    double scaleTex=1.0;
    bool pyramid=arguments.read("-pyr");
    int jpegQuality=95;
    arguments.read("--jpeg-quality",jpegQuality);
    bool useDisk=arguments.read("--outofcore");
    arguments.read("--scale",scaleTex);

    osg::Vec2 vtSize(-1,-1);
    arguments.read("--vt",vtSize.x(),vtSize.y());

    string imageName,depthName;
    unsigned int _tileRows;
    unsigned int _tileColumns;
    int row;
    int col;
    char tmp[1024];
    if(!arguments.read("--image",row,col,_tileRows,_tileColumns)){
        fprintf(stderr,"Fail to get image params\n");
        return -1;
    }
    int mosaic=-1;
    if(!arguments.read("--mosaicid",mosaic)){
        fprintf(stderr,"Fail to get mosaic id\n");
        return -1;
    }
    string global_scalefile;
    if(!arguments.read("-scalefile",global_scalefile)){
        fprintf(stderr,"Fail to get global_scalefile\n");
        return -1;
    }
    float global_scaleVal=1.0;
    FILE *ffp=fopen(global_scalefile.c_str(), "r");
    fscanf(ffp,"%f\n",&global_scaleVal);
    fclose(ffp);
   /* std::string matfile;
    if(!arguments.read("--mat",matfile)){
        fprintf(stderr,"Fail mat file\n");
        return -1;
    }*/
    mat4x  viewProjReadA ;
    mat4x  viewProjRemapped ;
   /* osg::Vec2 positions[4];
    osg::Vec3 normals[4];
    osg::Vec2 texcoords[4];
*/
    SamplerData sData;
    osg::Matrixd viewProjRead;
  /*  std::fstream _file(matfile.c_str(),std::ios::binary|std::ios::in);
    if(!_file.good()){
        fprintf(stderr,"Can't load %s\n",matfile.c_str());
        exit(-1);
    }
    for(int i=0; i<4; i++)
        for(int j=0; j<4; j++){
            _file.read(reinterpret_cast<char*>(&(viewProjRead(i,j))),sizeof(double));
            viewProjReadA.elem[j][i]=fixed16_t(viewProjRead(i,j));
        }
    mat4x *viewProjMats[2]={&viewProjRemapped,&viewProjReadA};*/
    mat4x *viewProjMats[2]={&viewProjRemapped,&viewProjReadA};
    sprintf(tmp,"%s/image_r%04d_c%04d_rs%04d_cs%04d",diced_img_dir,row,col,_tileRows,_tileColumns);

    imageName=string(tmp)+".v";
    depthName=string(tmp)+"-tmp_dist.v";
    double lat=0,lon=0;
    if(!arguments.read("-lat",lat)|| !arguments.read("-lon",lon)){
        fprintf(stderr,"Can't get lat long\n");
        return -1;
    }
    bool blending = arguments.read("--blend");
    if(blending)
        printf("Blending Enabled\n");
    string remappedName;
    if(!arguments.read("--remap",remappedName)){
        fprintf(stderr,"-remap must be passed with model name\n");
        exit(-1);
    }
    map<int,imgData> imageList;
    enum{ORIG_MAPPING,REMAPPED_MAPPING};

    tmpmodel= vertexData[ORIG_MAPPING].readPlyFile(av[1],false,NULL,DUP,true);
    if(!tmpmodel.valid()){
        fprintf(stderr,"can't load %s\n",av[1]);
        exit(-1);
    }
    tmpmodel= vertexData[REMAPPED_MAPPING].readPlyFile(remappedName.c_str(),false,NULL,DUP,true);
    if(!tmpmodel.valid()){
        fprintf(stderr,"can't load %s\n",remappedName.c_str());
        exit(-1);
    }
    readFile(av[2],imageList);
    if(tmpmodel.valid()){
        osg::Timer_t t=osg::Timer::instance()->tick();

        std::vector<osg::Vec3Array *>   texCoord;
        texCoord.push_back(vertexData[ORIG_MAPPING]._texCoord[0]);
        texCoord.push_back(vertexData[ORIG_MAPPING]._texCoord[1]);
        texCoord.push_back(vertexData[ORIG_MAPPING]._texCoord[2]);
        texCoord.push_back(vertexData[ORIG_MAPPING]._texCoord[3]);
        osg::DrawElementsUInt *tri=vertexData[ORIG_MAPPING]._triangles.get();

        int sizeX,sizeY;
        /*if(!arguments.read("--size",sizeX,sizeY)){
            osg::Vec2 srcsize;
            if(!arguments.read("--srcsize",srcsize.x(),srcsize.y())){
                fprintf(stderr,"need to have a src image size\n");
                exit(-1);
            }
            int sizeImage=  calcOptimalImageSizeRaw(srcsize,vertexData[REMAPPED_MAPPING]._vertices,tri,texCoord,scaleTex,(int)vtSize.x(),(int)vtSize.y());
            sizeX=sizeY=sizeImage;
            cout <<"Size "<< sizeX << "x"<<sizeY<<endl;
        }*/
        int origX,origY;
        char tmp11[8192];
        osg::Vec2 minTC,maxTC;

        sprintf(tmp11,"%s/image_r%04d_c%04d_rs%04d_cs%04d-remap.size.txt",diced_dir,row,col,_tileRows,_tileColumns);
        FILE *fp=fopen(tmp11,"r");
        if(!fp){
            fprintf(stderr,"Can't open %s\n",tmp11);
            exit(-1);
        }
        int ret=fscanf(fp,"%f %f %f %f %d %d %d %d\n",&(minTC.x()),&(minTC.y()),&(maxTC.x()),&(maxTC.y()),&origX,&origY,&sizeX,&sizeY);
        if(ret != 8){
            fprintf(stderr,"Can't parse %s\n",tmp11);
            exit(-1);
        }
        fclose(fp);
        cout <<"Size "<< sizeX << "x"<<sizeY<<endl;
        sizeX=ceil(sizeX*global_scaleVal);
        sizeY=ceil(sizeY*global_scaleVal);
        cout <<"Globally scaled to " << sizeX << "x"<<sizeY<<endl;
#if VIPS_MINOR_VERSION > 24

        vips_init(av[0]);
#endif
        osg::Vec2 texSize(sizeX,sizeY);
        for(int i=0; i<passesFlatRemap; i++){
            char tmp12[1024];
            sprintf(tmp12,"tmp%d",i);
            outputImage[i]=im_open(tmp12,"p");
            if(im_black(outputImage[i],sizeX,sizeY,4)){
                fprintf(stderr,"Can't create color image\n");
                return -1;
            }
        }
        gRect.width=128;
        gRect.height=1;

        /*  if(useDisk){
            IMAGE *diskFile=im_open(imageName.c_str(),"w");
            im_copy(outputImage,diskFile);
            im_close(outputImage);
            outputImage=diskFile;
        }
*/
        if(blending){
            for(int i=0; i<passesFlatRemap; i++){
                char tmp12[1024];
                sprintf(tmp12,"tmp_range%d",i);
                rangeImage[i]=im_open(tmp12,"p");
                if(im_white(rangeImage[i],sizeX,sizeY,4)){
                    fprintf(stderr,"Can't create range image\n");
                    return -1;
                }
            }/*
            if(useDisk){

                IMAGE *diskFileRange=im_open(depthName.c_str(),"w");
                im_copy(rangeImage,diskFileRange);
                im_close(rangeImage);
                rangeImage=diskFileRange;
            }*/
        }





        double vm, rss;
        process_mem_usage(vm, rss);
        cout << "1st VM: " << get_size_string(vm) << "; RSS: " << get_size_string(rss) << endl;
        for(int i=0; i<passesFlatRemap; i++){

            if(  im_rwcheck(outputImage[i]) ){
                fprintf(stderr,"can't open\n");
                return -1;
            }

            regOutput[i] = im_region_create(outputImage[i]);
            if(regOutput[i] == NULL){
                fprintf(stderr,"Can't init color image region\n");
                exit(-1);
            }

            if(blending){
                if(  im_rwcheck(rangeImage[i]) ){
                    fprintf(stderr,"can't open\n");
                    return -1;
                }


                regRange[i]  = im_region_create(rangeImage[i]);
                if(regRange[i] == NULL){
                    fprintf(stderr,"Can't init range image region\n");
                    exit(-1);
                }
            }
        }
        process_mem_usage(vm, rss);
        cout << "snd VM: " << get_size_string(vm) << "; RSS: " << get_size_string(rss) << endl;

        double s_d=osg::Timer::instance()->delta_s(t,osg::Timer::instance()->tick());
        printf("To reparam %f\n",s_d);


       /* osg::Geode *geode= dynamic_cast<osg::Geode*>(model.get());
        if(!geode)
            geode=model->asGroup()->getChild(0)->asGeode();
        else{
            //printf("fail\n");
        }
        if(geode && geode->getNumDrawables()){
            // printf("valid\n");
        }else{
            printf("Fail2\n");
            exit(-1);

        }
        osg::Geometry *geom=geode->getDrawable(0)->asGeometry();
        if(!geom){
            printf("No embedded geom\n");
            exit(-1);

        }
        float rx, ry, rz;
        osg::Matrix inverseM=osg::Matrix::identity();

        if(arguments.read("--invrot",rx,ry,rz)){
            inverseM =osg::Matrix::rotate(
                        osg::DegreesToRadians( rx ), osg::Vec3( 1, 0, 0 ),
                        osg::DegreesToRadians( ry ), osg::Vec3( 0, 1, 0 ),
                        osg::DegreesToRadians( rz ), osg::Vec3( 0, 0, 1 ) );
        }
        osg::Matrix rotM=osg::Matrix::inverse(inverseM);

        assert(verts->size() == vertexData._vertices->size());
        osg::BoundingBox totalbb;

        for(int i=0; i <(int)verts->size(); i++){
            verts->at(i)=verts->at(i)*rotM;
            totalbb.expandBy(verts->at(i));
        }
        cout << totalbb._min << " " <<totalbb._max<<endl;*/
        /*osg::Vec3d eye(totalbb.center()+osg::Vec3(0,0,3.5*totalbb.radius()));
        double xrange=totalbb.xMax()-totalbb.xMin();
        double yrange=totalbb.yMax()-totalbb.yMin();
        double largerSide=std::max(xrange,yrange);*/
        /*        osg::Matrixd matrix;
        matrix.makeTranslate( eye );

        osg::Matrixd view=osg::Matrix::inverse(matrix);
*/
        osg::Vec3Array *verts=vertexData[ORIG_MAPPING]._vertices;

        // osg::Matrixd view,proj;
        // mat4x viewA,projA;

        /*   std::fstream _file("view.mat",std::ios::binary|std::ios::in);
        for(int i=0; i<4; i++)
            for(int j=0; j<4; j++){
            _file.read(reinterpret_cast<char*>(&(view(i,j))),sizeof(double));
            viewA.elem[j][i]=fixed16_t(view(i,j));
        }
        for(int i=0; i<4; i++)
            for(int j=0; j<4; j++){
            _file.read(reinterpret_cast<char*>(&(proj(i,j))),sizeof(double));
            projA.elem[j][i]=fixed16_t(proj(i,j));

        }
        _file.close();
*/      vips::VImage colorOutput(outputImage[0]);
        VipsSampler viSamp(colorOutput);

        osg::Matrixd view,proj;
        osg::Vec3d eye(0.5,0.5,0);//totalbb.center()+osg::Vec3(0,0,3.5*totalbb.radius()));
        double xrange=1.0;//totalbb.xMax()-totalbb.xMin();
        double yrange=1.0;//totalbb.yMax()-totalbb.yMin();
        double largerSide=1.0;
        osg::Matrixd matrix;
        matrix.makeTranslate( eye );
        view=osg::Matrix::inverse(matrix);
        proj= osg::Matrixd::ortho2D(-(largerSide/2.0),(largerSide/2.0),-(largerSide/2.0),(largerSide/2.0));
        osg::Matrix viewproj=view*proj;
        osg::Matrix bottomLeftToTopLeft= (osg::Matrix::scale(1,-1,1)*osg::Matrix::translate(0,sizeY,0));

        osg::Matrix toTex=viewproj*( osg::Matrix::translate(1.0,1.0,1.0)*osg::Matrix::scale(0.5*sizeX,0.5*sizeY,0.5f))*bottomLeftToTopLeft;

        // cout <<viewProjRead<<endl;
        /*osg::Vec2Array *newTCArr=new osg::Vec2Array;
        for(int i=0; i <(int)verts->size(); i++){
            osg::Vec2 tc=calcCoordReprojSimple(verts->at(i),rotM,toTex,texSize);
            //     cout << "v: " << verts->at(i)<< " :" << tc<<endl;

            newTCArr->push_back(osg::Vec2(1.0-tc[1],1.0-tc[0]));
        }
        geom->setTexCoordArray(0,newTCArr);
        */// set up the texture state.

        for(int i=0; i<4; i++)
            for(int j=0; j<4; j++){
                viewProjRemapped.elem[j][i]=fixed16_t(viewproj(i,j));
            }
        //  cout << viewproj<<endl;
        //        printf("AAAA %d %d %d %d\n",row,col,_tileRows,_tileColumns);

        /*   osg::Matrix offsetMatrix=osg::Matrix::scale((double)_tileColumns,(double) _tileRows, 1.0)*osg::Matrix::translate((double)_tileColumns-1-2*col, (double)_tileRows-1-2*row, 0.0);


        mat4x offsetMatrixA=translation_matrix<fixed16_t>((double)_tileColumns-1-2*col, (double)_tileRows-1-2*row, 0.0)*scaling_matrix<fixed16_t>((double)_tileColumns,(double) _tileRows, 1.0);

        //mat4x viewA2=fast_inverse<fixed16_t>(translation_matrix<fixed16_t>(eye.x(),eye.y(),eye.z()));
        //osg::Matrixd proj= osg::Matrixd::ortho2D(-(largerSide/2.0),(largerSide/2.0),-(largerSide/2.0),(largerSide/2.0));

        //mat4x projA2=ortho_matrix<fixed16_t>(-(largerSide/2.0),(largerSide/2.0),-(largerSide/2.0),(largerSide/2.0),totalbb.zMin(),totalbb.zMax());
        osg::Matrixd viewprojmat=view*(proj*offsetMatrix);
        mat4x viewprojmatA=(offsetMatrixA*projA)*viewA;
        for(int i=0; i<4; i++){
            for(int j=0; j<4; j++){
                cout << fix2float<16>(viewprojmatA.elem[i][j].intValue) << " ";
            }
            cout <<endl;
        }
        cout <<endl;

        for(int i=0; i<4; i++){
            for(int j=0; j<4; j++){
                cout << fix2float<16>(viewProjReadA.elem[i][j].intValue) << " ";
            }
            cout <<endl;
        }
        cout <<endl;
        cout <<"OSG\n";
        cout <<viewprojmat<< endl;

        cout <<viewProjRead<< endl;*/
        /*
        for(int i=0; i<4; i++){
            for(int j=0; j<4; j++){
                cout << fix2float<16>(viewA2.elem[i][j].intValue) << " ";
            }
            cout <<endl;
        }
        cout <<endl;*/

        start = osg::Timer::instance()->tick();



        unsigned indices[] = {0, 1, 2};
        GeometryProcessor* g[2]={NULL,NULL};
        RasterizerSubdivAffine* r[2]={NULL,NULL};

        for(int i=0; i<passesFlatRemap; i++){

            // create a rasterizer class that will be used to rasterize primitives
            RasterizerSubdivAffine *rtmp =new RasterizerSubdivAffine;
            // create a geometry processor class used to feed vertex data.
            GeometryProcessor *g_i= new GeometryProcessor(rtmp);
            // it is necessary to set the viewport
            g_i->viewport(0, 0, outputImage[i]->Xsize, outputImage[i]->Ysize);
            // set the cull mode (CW is already the default mode)
            g_i->cull_mode(GeometryProcessor::CULL_NONE);

            // it is also necessary to set the clipping rectangle
            rtmp->clip_rect(0, 0, outputImage[i]->Xsize, outputImage[i]->Ysize);

            // set the vertex and fragment shaders


            // specify where out data lies in memory
            g_i->vertex_attrib_pointer(0, sizeof(Vertex), tmp_vertices);
            g[i]=g_i;
            r[i]=rtmp;
            doubleTouchCount[i]=0;
        }
        if(!vertexData[ORIG_MAPPING]._texCoord.size()){
            fprintf(stderr,"No tex coords\n");
            exit(-1);
        }
        osg::Vec2Array *newTCArr=new osg::Vec2Array;
        newTCArr->resize(vertexData[REMAPPED_MAPPING]._vertices->size());
        for(int i=0; i< (int)vertexData[REMAPPED_MAPPING]._triangles->size(); i++){
            osg::Vec2  tc2=osg::Vec2(vertexData[REMAPPED_MAPPING]._vertices->at(vertexData[REMAPPED_MAPPING]._triangles->at(i))[0],
                                     vertexData[REMAPPED_MAPPING]._vertices->at(vertexData[REMAPPED_MAPPING]._triangles->at(i))[1]);
            newTCArr->at(vertexData[REMAPPED_MAPPING]._triangles->at(i))=tc2;
        }


        if(0){
            osg::Texture2D* texture = new osg::Texture2D;
            osg::Image *img=new osg::Image;
            img->setWriteHint(osg::Image::EXTERNAL_FILE);
            img->setFileName(osgDB::getNameLessExtension(imageName)+"-tmp.ppm");
            texture->setDataVariance(osg::Object::DYNAMIC); // protect from being optimized away as static state.
            texture->setImage(img);
            osg::Geode *tmpGeode=new osg::Geode;
            osg::Geometry *tmpgeom=new osg::Geometry;
            tmpGeode->addDrawable(tmpgeom);
            tmpgeom->addPrimitiveSet(vertexData[ORIG_MAPPING]._triangles);
            tmpgeom->setVertexArray(vertexData[ORIG_MAPPING]._vertices);
            //tmpgeom->setTexCoordArray(0,newTCArr);
            osg::StateSet* stateset = tmpgeom->getOrCreateStateSet();
            stateset->setTextureAttributeAndModes(0,texture,osg::StateAttribute::ON);
            stateset->setMode( GL_LIGHTING, osg::StateAttribute::PROTECTED | osg::StateAttribute::OFF );

            osgDB::writeNodeFile(*tmpGeode,"hawk.ive");
            printf("%d %d %d\n",vertexData[ORIG_MAPPING]._triangles->size(),vertexData[ORIG_MAPPING]._vertices->size(),vertexData[REMAPPED_MAPPING]._vertices->size());
        }
       /* {

            std::vector<int>imageId;
            if(vertexData[ORIG_MAPPING]._texIds.valid()){
                for(int i=0; i< (int)tri->size()-2 && i < (int)vertexData[ORIG_MAPPING]._texIds->size(); i+=3){
                    imageId.push_back((int)vertexData[ORIG_MAPPING]._texIds->at(i)[0]);
                }
            }else{
                fprintf(stderr,"Failed to load texIDS\n");
                exit(-1);
            }
            char tmp[1024];
            sprintf(tmp,"%s/remap-%s",diced_dir,osgDB::getSimpleFileName(av[1]).c_str());
            std::ofstream f(tmp);
            bool color = vertexData[ORIG_MAPPING]._colors.valid() ? (vertexData[ORIG_MAPPING]._colors->size() >0) : false;

            if(vertexData[ORIG_MAPPING]._qualArray->size()*3 != vertexData[ORIG_MAPPING]._triangles->size()){
                fprintf(stderr,"Fail size not equal %d %d\n",vertexData[ORIG_MAPPING]._qualArray->size()*3 , vertexData[ORIG_MAPPING]._triangles->size());
                exit(-1);
            }

            int facecount=0;
            for(int i=0; i< (int)vertexData[ORIG_MAPPING]._triangles->size()-2; i+=3){
                if(vertexData[ORIG_MAPPING]._qualArray->at(i/3).x() == -999.0)
                    continue;
                facecount+=3;
            }
            printf("fc %d %d\n ",facecount,vertexData[ORIG_MAPPING]._triangles->size());
            write_header(f,facecount,color);

            osg::Vec4Array *colorArr=color ? vertexData[ORIG_MAPPING]._colors.get() : NULL;
            osg::Vec2Array *validArr=(vertexData[ORIG_MAPPING]._qualArray.valid() && vertexData[ORIG_MAPPING]._qualArray->size()>0) ? vertexData[ORIG_MAPPING]._qualArray : NULL;
            if(!colorArr || !validArr){
                fprintf(stderr,"Can't load ColorArr is null %d or validArr is null %d\n",(colorArr==NULL),(validArr == NULL));
                exit(-1);
            }
            write_all(f,vertexData[ORIG_MAPPING]._triangles,vertexData[ORIG_MAPPING]._vertices,colorArr,imageId,newTCArr,mosaic,true,validArr);

            f.close();
        }*/

        TextureMipMap *sizeI=NULL;

        if(blending){
            map<int,vector<ply::tri_t> >::iterator itr=vertexData[ORIG_MAPPING]._img2tri.begin();
            while((!sizeI || sizeI->surface==NULL) && itr!=vertexData[ORIG_MAPPING]._img2tri.end()){
                string tmp=string(string(av[3])+string("/")+imageList[itr->first].filename);

                sizeI=new TextureMipMap(tmp);
                itr++;
            }
        }
        //  return 0;
        for(int i=0; i<passesFlatRemap; i++){
            if(!blending){
                g[i]->vertex_shader<VertexShader>();
                r[i]->fragment_shader<FragmentShader>();
            }
            else{
                g[i]->vertex_shader<VertexShaderBlending>();
                r[i]->fragment_shader<FragmentShaderBlendingDistPass>();
                VertexShaderBlending::texture =sizeI;
                FragmentShaderBlendingDistPass::texture =sizeI;
            }
        }



        for(int i=0;i <passesFlatRemap; i++){

            FragmentShaderBlendingDistPass::regRange =regRange[i];
            FragmentShaderBlendingDistPass::doublecountmapPtr =&(doublecountmap[i]);
            FragmentShaderBlendingDistPass::doubleTouchCountPtr =&(doubleTouchCount[i]);
            viSamp.regOutput=regOutput[i];
            viSamp.regRange=regRange[i];
            viSamp.doublecountmapPtr =&(doublecountmap[i]);
            //viSamp.texture =textureMipMap;
            if(!blending)
                VertexShader::modelviewprojection_matrix=(*viewProjMats[i]);
            else{
                VertexShaderBlendingDistPass::modelviewprojection_matrix=(*viewProjMats[i]);
                VertexShaderBlending::modelviewprojection_matrix=(*viewProjMats[i]);
            }


            for( map<int,vector<ply::tri_t> >::iterator itr=vertexData[ORIG_MAPPING]._img2tri.begin(); itr!=vertexData[ORIG_MAPPING]._img2tri.end(); itr++){
                for(int t=0; t< (int)itr->second.size(); t++){
                    osg::Vec3Array *use_vert= (i==0) ? verts : vertexData[ORIG_MAPPING]._vertices.get() ;
                    int aliasing;
                    osg::Matrix *proj;
                    int mapping;
                    if(i == 0){
                        mapping=REMAPPED_MAPPING;
                        aliasing=RASTER_ANTIALIAS;
                        proj=NULL;
                    }else{
                        aliasing=RASTER_NOAA;
                        mapping=ORIG_MAPPING;
                        proj=&viewProjRead;
                    }
                    if(process_tri_new(itr->second[t],vertexData[mapping]._vertices,vertexData[mapping]._texCoord,blending,sizeX,sizeY,viSamp,sData,proj)){
                        Raster::drawTriangle(aliasing, texSize, sData.positions,sData.texcoords,
                                             VipsSampler::renderDepthTriCallback, &viSamp);
                    }


                }
            }


            printf("Double touch count %d\n",doubleTouchCount[i]);

            g[i]->vertex_shader<VertexShaderBlending>();
            r[i]->fragment_shader<FragmentShaderBlendingMain>();

        }

        if(sizeI){
            delete sizeI;
        }
        unsigned int total_tri_count=0,count=0;
        for( map<int,vector<ply::tri_t> >::iterator itr=vertexData[ORIG_MAPPING]._img2tri.begin(); itr!=vertexData[ORIG_MAPPING]._img2tri.end(); itr++)
            total_tri_count+=itr->second.size();

        for( map<int,vector<ply::tri_t> >::iterator itr=vertexData[ORIG_MAPPING]._img2tri.begin(); itr!=vertexData[ORIG_MAPPING]._img2tri.end(); itr++){
            string tmp=string(string(av[3])+string("/")+imageList[itr->first].filename);

            Texture *texture=NULL;
            TextureMipMap *textureMipMap=NULL;

            if(!blending){
                texture = new Texture(tmp);
                FragmentShader::texture = texture;
                VertexShader::texture = texture;
                if(!texture->surface)
                    continue;
            }else{
                textureMipMap = new TextureMipMap(tmp,itr->first);
                FragmentShaderBlendingMain::texture = textureMipMap;
                VertexShaderBlending::texture =textureMipMap;
                if(!textureMipMap->surface)
                    continue;

            }
            for(int i=0;i <passesFlatRemap; i++){
                if(!blending)
                    VertexShader::modelviewprojection_matrix=(*viewProjMats[i]);
                else{
                    VertexShaderBlendingDistPass::modelviewprojection_matrix=(*viewProjMats[i]);
                    VertexShaderBlending::modelviewprojection_matrix=(*viewProjMats[i]);
                }
                FragmentShaderBlendingMain::regOutput=regOutput[i];
                FragmentShaderBlendingMain::regRange=regRange[i];
                FragmentShaderBlendingMain::doublecountmapPtr =&(doublecountmap[i]);
                viSamp.regOutput=regOutput[i];
                viSamp.regRange=regRange[i];
                viSamp.doublecountmapPtr =&(doublecountmap[i]);
                viSamp.texture =textureMipMap;
                for(int t=0; t< (int)itr->second.size(); t++){
                    if(i==1){
                        if(count % 300 == 0){
                            printf("\r %02d%%: %d/%d",(int)(100.0*(count/(float)total_tri_count)),count,total_tri_count);
                            fflush(stdout);
                        }
                        count++;
                    }
                    if(!blending && itr->second[t].pos !=0 )
                        continue;
                    osg::Vec3Array *use_vert= (i==0) ? verts : vertexData[ORIG_MAPPING]._vertices.get() ;
                    int aliasing;
                    osg::Matrix *proj;
                    int mapping;
                    if(i == 0){
                        mapping=REMAPPED_MAPPING;
                        aliasing=RASTER_ANTIALIAS;
                        proj=NULL;
                    }else{
                        aliasing=RASTER_NOAA;
                        mapping=ORIG_MAPPING;
                        proj=&viewProjRead;
                    }
                    if(process_tri_new(itr->second[t],vertexData[mapping]._vertices,vertexData[mapping]._texCoord,blending,sizeX,sizeY,viSamp,sData,proj)){
                        Raster::drawTriangle(aliasing, texSize, sData.positions,sData.texcoords,
                                             VipsSampler::renderBlendedTriCallback, &viSamp);
                    }
                }
            }
            if(!blending){
                delete texture;

            }
            else{
                delete textureMipMap;
            }

        }
        printf("\r %02d%%: %d/%d\n",(int)(100.0*(count/(float)total_tri_count)),count,total_tri_count);
        fflush(stdout);
        double elapsed=osg::Timer::instance()->delta_s(start,osg::Timer::instance()->tick());
        std::cout << "\n"<<format_elapsed(elapsed) << std::endl;
        process_mem_usage(vm, rss);
        cout << "VM: " << get_size_string(vm) << "; RSS: " << get_size_string(rss) << endl;
        for(int i=0; i<passesFlatRemap; i++){
            im_region_free(regOutput[i]);

            if(blending){
                im_region_free(regRange[i]);
                im_close(rangeImage[i]);

                /*       if(useDisk){
                if( remove( depthName.c_str() ) != 0 )
                    perror( "Error deleting file" );
                else
                    puts( "File successfully deleted" );
            }*/
            }
        }
        process_mem_usage(vm, rss);
        cout << "VM: " << get_size_string(vm) << "; RSS: " << get_size_string(rss) << endl;

        start=osg::Timer::instance()->tick();
        //(osgDB::getNameLessExtension(imageName)+"-tmp.tif:packbits,tile:256x256").c_str()
        IMAGE *tmpI=im_open("tmp","p");
        im_extract_bands(outputImage[0],tmpI,0,3);
        string remapName=(osgDB::getNameLessExtension(imageName)+"-remap.ppm");
        dilateEdge(tmpI,remapName.c_str(),1);
        /*if( im_vips2ppm(tmpI,(osgDB::getNameLessExtension(imageName)+"-remap.ppm").c_str())){
            fprintf(stderr,"Failed to write\n");
            cerr << im_error_buffer()<<endl;
            im_close(tmpI);
            exit(-1);
        }*/
        im_close(tmpI);
        int levels=(int)ceil(log( min( sizeX, sizeY ))/log(2.0) );
        if(pyramid){ if(!genPyramid(remapName,basepath,levels,sizeX,sizeY,"ppm")){
                fprintf(stderr,"FAil to gen pyramid\n");
                exit(-1);
            }
        }
        if(passesFlatRemap > 1){

            tmpI=im_open("tmp","p");
            im_extract_bands(outputImage[1],tmpI,0,3);

            if( im_vips2ppm(tmpI,(osgDB::getNameLessExtension(imageName)+"-tmp.ppm").c_str())){
                fprintf(stderr,"Failed to write\n");
                cerr << im_error_buffer()<<endl;
                im_close(tmpI);
                exit(-1);
            }
            /*  vips::VImage maskI(tmpI);
        vips::VImage dilatedI(tmpI);
        const int size=4;
        std::vector<int> coeff(size*size,255);
        vips::VIMask mask(size,size,1,0,coeff);
        vips::VIMask mask(size,size,1,0,coeff);

        dilatedI.dilate(mask2).write("test.png");

        maskI.more(1.0).invert().andimage(dilatedI.dilate(mask)).write("wa.ppm");
        (maskI.more(1.0).invert().andimage(dilatedI.dilate(mask))).add(maskI).write("total.png");*/
            im_close(tmpI);



            IMAGE *tmpI2=im_open("tmp2","p");
            im_extract_bands(outputImage[1],tmpI2,3,1);
            if( im_vips2ppm(tmpI2,(osgDB::getNameLessExtension(imageName)+"-tmp-mask.pgm").c_str())){
                fprintf(stderr,"Failed to write\n");
                cerr << im_error_buffer()<<endl;
                im_close(tmpI2);
                exit(-1);
            }
            im_close(tmpI2);
        }
        elapsed=osg::Timer::instance()->delta_s(start,osg::Timer::instance()->tick());
        std::cout << "\n"<<format_elapsed(elapsed) << std::endl;
        process_mem_usage(vm, rss);
        cout << "VM: " << get_size_string(vm) << "; RSS: " << get_size_string(rss) << endl;
        for(int i=0; i<passesFlatRemap; i++)
            im_close(outputImage[i]);
        if(passesFlatRemap > 1){
            if(applyGeoTags(osgDB::getNameLessExtension(imageName)+".tif",osg::Vec2(lat,lon),viewProjRead,sizeX,sizeY,basepath,"ppm",jpegQuality)){
                /* if( remove((osgDB::getNameLessExtension(imageName)+"-tmp.tif").c_str() ) != 0 )
                perror( "Error deleting file" );
            else
                puts( "File successfully deleted" );*/
            }
        }


        if(useDisk){
            if( remove( imageName.c_str() ) != 0 )
                perror( "Error deleting file" );
            else
                puts( "File successfully deleted" );
        }

    }


    return 0;
}



using namespace vcg;
using namespace std;


#define AMesh CMeshO

/*
// Vertex, Face, Mesh and Grid definitions.
class MyEdge;
class AFace;
class AVertex   : public VertexVCVN< float ,MyEdge,AFace > {};
class AFace     : public FaceFN< AVertex,MyEdge,AFace > {};
class AMesh     : public tri::TriMesh< vector<AVertex>, vector<AFace> > {};*/
#include "meshmodel.h"

#include "sw-visshader.h"
///////// Global ////////

int SampleNum=64;
int WindowRes=800;
unsigned int TexInd=0;
bool SwapFlag=false;
bool CullFlag=false;
bool ClosedFlag=false;
Point3f ConeDir(0,1,0);
float ConeAngleRad = math::ToRad(180.0f);

float lopass=0,hipass=1,gamma_correction=1;
float diff=.8;
float ambi=.2;
double MM[16];
double MP[16];
int VP[4];

bool LightFlag=true;
bool ColorFlag=true;
bool FalseColorFlag=false;
bool ShowDirFlag=false;
int imgcnt=0;

Color4b BaseColor=Color4b::White;
#ifdef USEGL

Trackball QV;
Trackball QL;
Trackball *Q=&QV;
#endif
int ScreenH,ScreenW;
float ViewAngle=33;
vector<Point3f> ViewVector;



AMesh m;
VertexVisShader<AMesh> Vis(m);

string OutNameMsh;

bool reorderVertsForTex(void);

osg::Vec3Array * doMeshReorder(std::string basename)
{

    int i=1;



    if(!(basename.substr(basename.length()-4)==".ply"))	{
        printf("Error: Unknown file extension %s\n",basename.c_str());
        return NULL;
    }

    // loading original mesh
    int ret=tri::io::ImporterPLY<AMesh>::Open(m,basename.c_str());
    if(ret) {printf("Error unable to open mesh %s : '%s' \n",basename.c_str(),tri::io::ImporterPLY<AMesh>::ErrorMsg(ret));exit(-1);}
    if(m.fn == 0){
        printf("No faces empty mesh \n");
        //return 1;
    }else{

        if(SwapFlag){
            printf("Flipping normal\n");
            tri::Clean<CMeshO>::FlipMesh(m);

        }
        tri::UpdateNormals<AMesh>::PerVertexNormalized(m);
        tri::UpdateBounding<AMesh>::Box(m);
        tri::UpdateColor<AMesh>::VertexConstant(m,Color4b::White);


        if(!tri::HasPerWedgeTexCoord(m))
        {
            printf("Warning: nothing have been done. Mesh has no Texture.\n");
            return NULL;
        }
        if ( ! tri::Clean<CMeshO>::HasConsistentPerWedgeTexCoord(m) ) {
            printf( "Mesh has some inconsistent tex coords (some faces without texture)\n");
        }
        if(!tri::HasPerWedgeColor(m))
        {
            printf("Warning:. Mesh has no Color\n");
        }
        printf("Mesh bbox (%f %f %f)-(%f %f %f)\n\n",
               m.bbox.min[0],m.bbox.min[1],m.bbox.min[2],
               m.bbox.max[0],m.bbox.max[1],m.bbox.max[2]);

        string path=(osgDB::getFilePath(string(basename.c_str())));
        if(path.size()==0 || path=="/")
            path=".";
        OutNameMsh="mov-"+osgDB::getSimpleFileName(basename);
        OutNameMsh=path+"/"+OutNameMsh;

        printf("Mesh       Output filename %s\n",OutNameMsh.c_str());

        printf("Mesh %iv %if bbox Diag %g\n",m.vn,m.fn,m.bbox.Diag());


    }
    if(!reorderVertsForTex()){
        fprintf(stderr,"Moving verts failed\n");
    }
    vcg::tri::io::PlyInfo p;
    //p.mask|=vcg::tri::io::Mask::IOM_VERTCOLOR  /* | vcg::ply::PLYMask::PM_VERTQUALITY*/ ;
    p.mask |= vcg::tri::io::Mask::IOM_WEDGTEXCOORD;
    p.mask |= vcg::tri::io::Mask::IOM_WEDGCOLOR;

    //tri::io::ExporterPLY<AMesh>::Save(m,OutNameMsh.c_str(),true,p);
    osg::Vec3Array *varr=new osg::Vec3Array;
    for(CMeshO::VertexIterator vi=m.vert.begin();vi!=m.vert.end();++vi)
        varr->push_back(osg::Vec3((*vi).P()[0],(*vi).P()[1],(*vi).P()[2]));

    //tri::io::ExporterPLY<AMesh>::Save(m,OutNameMsh.c_str(),false);
    //     exit(0);

    // glutMainLoop();

    return(varr);
}


bool removeDups(std::string basename,osg::Vec3Array *verts,osg::DrawElementsUInt * triangles)
{




    if(!(basename.substr(basename.length()-4)==".ply"))	{
        printf("Error: Unknown file extension %s\n",basename.c_str());
        return NULL;
    }

    // loading original mesh
    int ret=tri::io::ImporterPLY<AMesh>::Open(m,basename.c_str());
    if(ret) {printf("Error unable to open mesh %s : '%s' \n",basename.c_str(),tri::io::ImporterPLY<AMesh>::ErrorMsg(ret));exit(-1);}
    if(m.fn == 0){
        printf("No faces empty mesh \n");
        return false;
        //return 1;
    }

    /*  if(SwapFlag){
            printf("Flipping normal\n");
            tri::Clean<CMeshO>::FlipMesh(m);

        }*/
    double CCPerc=0.05;
    tri::UpdateNormals<AMesh>::PerVertexNormalized(m);
    tri::UpdateBounding<AMesh>::Box(m);
    //  tri::UpdateColor<AMesh>::VertexConstant(m,Color4b::White);
    int dup= tri::Clean<AMesh>::RemoveDuplicateVertex(m);


    /*
    tri::UpdateTopology<CMeshO>::FaceFace(m);
    tri::UpdateFlags<CMeshO>::FaceBorderFromFF(m);
    cout <<" NONmanifold edge "<<tri::Clean<AMesh>:: CountNonManifoldEdgeFF(m)<<endl;
    cout <<" Nonmanifold vertex "<<tri::Clean<AMesh>:: CountNonManifoldVertexFF(m,true)<<endl;
    tri::UpdateSelection<CMeshO>::FaceFromVertexLoose(m);
    CMeshO::FaceIterator   fi;
    CMeshO::VertexIterator vi;

    for(fi=m.face.begin();fi!=m.face.end();++fi)
        if(!(*fi).IsD() && (*fi).IsS() )
            tri::Allocator<CMeshO>::DeleteFace(m,*fi);

    for(AMesh::VertexIterator vi=m.vert.begin();vi!=m.vert.end();++vi){
        if(vi->IsS()){
            tri::Allocator<CMeshO>::DeleteVertex(m,*vi);

        }
    }
    tri::UpdateTopology<CMeshO>::FaceFace(m);
    tri::UpdateFlags<CMeshO>::FaceBorderFromFF(m);
    cout <<" NONmanifold edge "<<tri::Clean<AMesh>:: CountNonManifoldEdgeFF(m)<<endl;
    cout <<" Nonmanifold vertex "<<tri::Clean<AMesh>:: CountNonManifoldVertexFF(m,true)<<endl;*/



    /*
 int unref2= tri::Clean<AMesh>::RemoveNonManifoldFace(m);
    int unref= tri::Clean<AMesh>::RemoveNonManifoldVertex(m);
    int dup2= tri::Clean<AMesh>::RemoveDegenerateFace(m);
    tri::UpdateTopology<CMeshO>::FaceFace(m);

    cout <<" important "<<tri::Clean<AMesh>:: CountNonManifoldEdgeFF(m) <<" "<<tri::Clean<AMesh>:: CountNonManifoldVertexFF(m,true)<< endl;
    tri::UpdateSelection<CMeshO>::FaceFromVertexLoose(m);

    for(fi=m.face.begin();fi!=m.face.end();++fi)
        if(!(*fi).IsD() && (*fi).IsS() )
            tri::Allocator<CMeshO>::DeleteFace(m,*fi);



    tri::UpdateSelection<CMeshO>::ClearVertex(m);

    printf("Removed Non man %d %d\n",unref2,unref);
    unref= tri::Clean<AMesh>::RemoveUnreferencedVertex(m);
    tri::UpdateTopology<CMeshO>::FaceFace(m);

    cout <<" final "<<tri::Clean<AMesh>:: CountNonManifoldEdgeFF(m) <<" "<<tri::Clean<AMesh>:: CountNonManifoldVertexFF(m,true)<< endl;

    float minCC= CCPerc*m.bbox.Diag();
    printf("Cleaning Min CC %.1f m\n",minCC);
    std::pair<int,int> delInfo= tri::Clean<AMesh>::RemoveSmallConnectedComponentsDiameter(m,minCC);

    printf("fff %d %d\n",m.fn,m.vn);*/
    /*double CCPerc=0.2;

    float minCC= CCPerc*m.bbox.Diag();
    printf("Cleaning Min CC %.1f m\n",minCC);
    std::pair<int,int> delInfo= tri::Clean<AMesh>::RemoveSmallConnectedComponentsDiameter(m,minCC);*/
    /*  float minCC= CCPerc*m.bbox.Diag();
    printf("Cleaning Min CC %.1f m\n",minCC);
    std::pair<int,int> delInfo= tri::Clean<AMesh>::RemoveSmallConnectedComponentsDiameter(m,minCC);
    cout <<delInfo.first<<"/"<<delInfo.second<<endl;*/
    vcg::SimpleTempData<AMesh::VertContainer,int> indices(m.vert);

    int j=0;
    for(AMesh::VertexIterator vi=m.vert.begin();vi!=m.vert.end();++vi){
        if(!vi->IsD()){
            verts->push_back(osg::Vec3((*vi).P()[0],(*vi).P()[1],(*vi).P()[2]));
            indices[vi] = j++;
        }
    }
    printf("fff %d %d\n",(int)verts->size(),j);

    for(AMesh::FaceIterator fi=m.face.begin();fi!=m.face.end();++fi){
        if(fi->IsD()){
            for(int k=0;k<3;++k)
                triangles->push_back(-1);
        }else{
            for(int k=0;k<3;++k)
                triangles->push_back(indices[(*fi).cV(k)]);
        }
    }


    // vcg::tri::io::PlyInfo pi;

    //  vcg::tri::io::ExporterOBJ<AMesh>::Save(m,"nodup.obj",pi.mask);

    //tri::io::ExporterPLY<AMesh>::Save(m,OutNameMsh.c_str(),false);
    //     exit(0);

    // glutMainLoop();
    return true;
}
typedef Triangle2<CMeshO::FaceType::TexCoordType::ScalarType> Tri2;
/////// FUNCTIONS NEEDED BY "BASIC PARAMETRIZATION" FILTER
inline int getLongestEdge(const CMeshO::FaceType & f)
{
    int res=0;
    const CMeshO::CoordType &p0=f.cP(0), &p1=f.cP(1), p2=f.cP(2);
    double  maxd01 = SquaredDistance(p0,p1);
    double  maxd12 = SquaredDistance(p1,p2);
    double  maxd20 = SquaredDistance(p2,p0);
    if(maxd01 > maxd12)
        if(maxd01 > maxd20)     res = 0;
        else                    res = 2;
    else
        if(maxd12 > maxd20)     res = 1;
        else                    res = 2;
    return res;
}

inline void buildTrianglesCache(std::vector<Tri2> &arr, int maxLevels, float border, float quadSize, int idx=-1)
{
    assert(idx >= -1);
    Tri2 &t0 = arr[2*idx+2];
    Tri2 &t1 = arr[2*idx+3];
    if (idx == -1)
    {
        // build triangle 0
        t0.P(1).X() = quadSize - (0.5 + M_SQRT1_2)*border;
        t0.P(0).X() = 0.5 * border;
        t0.P(1).Y() = 1.0 - t0.P(0).X();
        t0.P(0).Y() = 1.0 - t0.P(1).X();
        t0.P(2).X() = t0.P(0).X();
        t0.P(2).Y() = t0.P(1).Y();
        // build triangle 1
        t1.P(1).X() = (0.5 + M_SQRT1_2)*border;
        t1.P(0).X() = quadSize - 0.5 * border;
        t1.P(1).Y() = 1.0 - t1.P(0).X();
        t1.P(0).Y() = 1.0 - t1.P(1).X();
        t1.P(2).X() = t1.P(0).X();
        t1.P(2).Y() = t1.P(1).Y();
    }
    else {
        // split triangle idx in t0 t1
        Tri2 &t = arr[idx];
        Tri2::CoordType midPoint = (t.P(0) + t.P(1)) / 2;
        Tri2::CoordType vec10 = (t.P(0) - t.P(1)).Normalize() * (border/2.0);
        t0.P(1) = t.P(0);
        t1.P(0) = t.P(1);
        t0.P(2) = midPoint + vec10;
        t1.P(2) = midPoint - vec10;
        t0.P(0) = t.P(2) + ( (t.P(0)-t.P(2)).Normalize() * border / M_SQRT2 );
        t1.P(1) = t.P(2) + ( (t.P(1)-t.P(2)).Normalize() * border / M_SQRT2 );
    }
    if (--maxLevels <= 0) return;
    buildTrianglesCache (arr, maxLevels, border, quadSize, 2*idx+2);
    buildTrianglesCache (arr, maxLevels, border, quadSize, 2*idx+3);
}

// ERROR CHECKING UTILITY
#define CheckError(x,y); if ((x)) {printf("%s\n",(y)); return false;}

bool reorderVertsForTex(void){
    // Get Parameters
    int sideDim = 0;
    int textDim = 4096;
    int pxBorder =8;
    bool adv=true;


    // Pre checks
    CheckError(textDim <= 0, "Texture Dimension has an incorrect value");
    CheckError(pxBorder < 0,   "Inter-Triangle border has an incorrect value");
    CheckError(sideDim < 0,  "Quads per line border has an incorrect value");

    if (adv) //ADVANCED SPACE-OPTIMIZING
    {
        float border = ((float)pxBorder) / textDim;

        // Creates a vector of double areas
        double maxArea = -1, minArea=DBL_MAX;
        std::vector<double> areas;
        int faceNo = 0;
        for (uint i=0; i<m.face.size(); ++i)
        {
            if (!m.face[i].IsD())
            {
                double area = DoubleArea(m.face[i]);
                if (area == 0) area = DBL_MIN;
                if (area > maxArea) maxArea = area;
                if (area < minArea) minArea = area;
                areas.push_back(area);
                ++faceNo;
            } else {
                areas.push_back(-1.0);
            }
        }

        // Creates buckets containing each halfening level triangles (a histogram)
        int    buckSize = (int)ceil(log2(maxArea/minArea) + DBL_EPSILON);
        std::vector<std::vector<uint> > buckets(buckSize);
        for (uint i=0; i<areas.size(); ++i)
            if (areas[i]>=0)
            {
                int slot = (int)ceil(log2(maxArea/areas[i]) + DBL_EPSILON) - 1;
                assert(slot < buckSize && slot >= 0);
                buckets[slot].push_back(i);
            }

        // Determines correct dimension and accordingly max halfening levels
        int dim = 0;
        int halfeningLevels = 0;

        double qn = 0., divisor = 2.0;
        int rest = faceNo, oneFact = 1, sqrt2Fact = 1;
        bool enough = false;
        while (halfeningLevels < buckSize)
        {
            int tmp =(int)ceil(sqrt(qn + rest/divisor));
            bool newenough = true;
            if (sideDim != 0)
            {
                newenough = sideDim>=tmp;
                tmp = sideDim;
            }

            // this check triangles dimension limit too
            if (newenough && 1.0/tmp < (sqrt2Fact/M_SQRT2 + oneFact)*border +
                    (oneFact != sqrt2Fact ? oneFact*M_SQRT2*2.0/textDim : oneFact*2.0/textDim)) break;

            enough = newenough;
            rest -= buckets[halfeningLevels].size();
            qn += buckets[halfeningLevels].size() / divisor;
            divisor *= 2.0;

            if (halfeningLevels%2)
                oneFact *= 2;
            else
                sqrt2Fact *= 2;

            dim = tmp;
            halfeningLevels++;
        }

        // Post checks
        CheckError(!enough && halfeningLevels==buckSize, ("Quads per line aren't enough to obtain a correct parametrization\nTry setting at least "));
        CheckError(halfeningLevels==0  || !enough, "Inter-Triangle border is too much");

        //Create cache of possible triangles (need only translation in correct position)
        std::vector<Tri2> cache((1 << (halfeningLevels+1))-2);
        buildTrianglesCache(cache, halfeningLevels, border, 1.0/dim);

        // Setting texture coordinates (finally)
        Tri2::CoordType origin;
        Tri2::CoordType tmp;
        int buckIdx=0, face=0;
        std::vector<uint>::iterator it = buckets[buckIdx].begin();
        int currLevel = 1;
        for (int i=0; i<dim && face<faceNo; ++i)
        {
            origin.Y() = -((float)i)/dim;
            for (int j=0; j<dim && face<faceNo; j++)
            {
                origin.X() = ((float)j)/dim;
                for (int pos=(1<<currLevel)-2; pos<(1<<(currLevel+1))-2 && face<faceNo; ++pos, ++face)
                {
                    while (it == buckets[buckIdx].end()) {
                        if (++buckIdx < halfeningLevels)
                        {
                            ++currLevel;
                            pos = 2*pos+2;
                        }
                        it = buckets[buckIdx].begin();
                    }
                    int fidx = *it;
                    int lEdge = getLongestEdge(m.face[fidx]);
                    Tri2 &t = cache[pos];


                    /*tmp = t.P(0) + origin;
                    m.face[fidx].WT(lEdge) = CFaceO::TexCoordType(tmp.X(), tmp.Y());
                    m.face[fidx].WT(lEdge).N() = 0;
                    lEdge = (lEdge+1)%3;
                    tmp = t.P(1) + origin;
                    m.face[fidx].WT(lEdge) = CFaceO::TexCoordType(tmp.X(), tmp.Y());
                    m.face[fidx].WT(lEdge).N() = 0;
                    lEdge = (lEdge+1)%3;
                    tmp = t.P(2) + origin;
                    m.face[fidx].WT(lEdge) = CFaceO::TexCoordType(tmp.X(), tmp.Y());
                    m.face[fidx].WT(lEdge).N() = 0;*/

                    tmp = t.P(0) + origin;
                    m.face[fidx].V(0)->P() = Point3f(tmp.X(), tmp.Y(),0);
                    //m.face[fidx].V(lEdge).N() = 0;
                    lEdge = (lEdge+1)%3;
                    tmp = t.P(1) + origin;
                    m.face[fidx].V(1)->P() = Point3f(tmp.X(), tmp.Y(),0);
                    // m.face[fidx].V(lEdge).N() = 0;
                    lEdge = (lEdge+1)%3;
                    tmp = t.P(2) + origin;
                    m.face[fidx].V(2)->P() =Point3f(tmp.X(), tmp.Y(),0);
                    // m.face[fidx].WT(lEdge).N() = 0;
                    /*  cout << (t.P(0) +origin)[0]<< ","<<(t.P(0) +origin)[1]<< endl;
                    cout << (t.P(1) +origin)[0]<< ","<<(t.P(1) +origin)[1]<< endl;
                    cout << (t.P(2) +origin)[0]<< ","<<(t.P(2) +origin)[1]<< endl;
cout << endl;*/
                    ++it;
                    //cb(face*100/faceNo, "Generating parametrization...");
                }
            }
        }
        assert(face == faceNo);
        assert(it == buckets[buckSize-1].end());
        printf( "Biggest triangle's catheti are %.2f px long", (cache[0].P(0)-cache[0].P(2)).Norm() * textDim);
        printf( "Smallest triangle's catheti are %.2f px long", (cache[cache.size()-1].P(0)-cache[cache.size()-1].P(2)).Norm() * textDim);

    }
    else //BASIC
    {
        //Get total faces and total undeleted face
        int faceNo = m.face.size();
        int faceNotD = 0;
        for (CMeshO::FaceIterator fi=m.face.begin(); fi!=m.face.end(); ++fi)
            if (!fi->IsD()) ++faceNotD;

        // Minimum side dimension to get correct halfsquared triangles
        int optimalDim = ceilf(sqrtf(faceNotD/2.));
        if (sideDim == 0) sideDim = optimalDim;
        else {
            CheckError(optimalDim > sideDim, ("Quads per line aren't enough to obtain a correct parametrization\nTry setting at least "));
        }

        //Calculating border size in UV space
        float border = ((float)pxBorder) / textDim;
        CheckError(border*(1.0+M_SQRT2)+2.0/textDim > 1.0/sideDim, "Inter-Triangle border is too much");

        float bordersq2 = border / M_SQRT2;
        float halfborder = border / 2;

        bool odd = true;
        CFaceO::TexCoordType botl, topr;
        int face=0;
        botl.V() = 1.;
        for (int i=0; i<sideDim && face<faceNo; ++i)
        {
            topr.V() = botl.V();
            topr.U() = 0.;
            botl.V() = 1.0 - 1.0/sideDim*(i+1);
            for (int j=0; j<2*sideDim && face<faceNo; ++face)
            {
                if (!m.face[face].IsD())
                {
                    int lEdge = getLongestEdge(m.face[face]);
                    if (odd) {
                        botl.U() = topr.U();
                        topr.U() = 1.0/sideDim*(j/2+1);
                        CFaceO::TexCoordType bl(botl.U()+halfborder, botl.V()+halfborder+bordersq2);
                        CFaceO::TexCoordType tr(topr.U()-(halfborder+bordersq2), topr.V()-halfborder);
                        bl.N() = 0;
                        tr.N() = 0;
                        m.face[face].WT(lEdge) = bl;
                        m.face[face].WT((++lEdge)%3) = tr;
                        m.face[face].WT((++lEdge)%3) = CFaceO::TexCoordType(bl.U(), tr.V());
                        m.face[face].WT(lEdge%3).N() = 0;
                    } else {
                        CFaceO::TexCoordType bl(botl.U()+(halfborder+bordersq2), botl.V()+halfborder);
                        CFaceO::TexCoordType tr(topr.U()-halfborder, topr.V()-(halfborder+bordersq2));
                        bl.N() = 0;
                        tr.N() = 0;
                        m.face[face].WT(lEdge) = tr;
                        m.face[face].WT((++lEdge)%3) = bl;
                        m.face[face].WT((++lEdge)%3) = CFaceO::TexCoordType(tr.U(), bl.V());
                        m.face[face].WT(lEdge%3).N() = 0;
                    }
                    //  cb(face*100/faceNo, "Generating parametrization...");
                    odd=!odd; ++j;
                }
            }
        }
        printf( "Triangles' catheti are %.2f px long", (1.0/sideDim-border-bordersq2)*textDim);
    }
    return true;

}

