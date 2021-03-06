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

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/io_utils>
#include "Clipper.h"
#include <osgUtil/SmoothingVisitor>
#include <osg/ComputeBoundsVisitor>
#include "PLYWriterNodeVisitor.h"
#include "TexturedSource.h"
#include "TexturingQuery.h"
#include "Extents.h"
#include "calcTexCoord.h"
#include "calibFile.h"
#include "PLYWriterNodeVisitor.h"
#include "GLImaging.h"
#include "vertexData.h"
using namespace std;

int main( int argc, char **argv )
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc,argv);

    // set up the usage document, in case we need to print out how to use this program.
    arguments.getApplicationUsage()->setDescription(arguments.getApplicationName() +" is the example which demonstrates Depth Peeling");
    arguments.getApplicationUsage()->setCommandLineUsage(arguments.getApplicationName()+" filename");
    ;
    string path=string(argv[0]);
    unsigned int loc=path.rfind("/");

    string basepath= loc == string::npos ? "./" : path.substr(0,loc+1);
    basepath= osgDB::getRealPath (basepath);
    string base_dir=argv[1];
    string stereo_calib_file_name = "stereo.calib";
    stereo_calib_file_name= base_dir+string("/")+stereo_calib_file_name;

    StereoCalib calib(stereo_calib_file_name.c_str());

    string outfilename;
    if(!arguments.read("--outfile",outfilename)){
        fprintf(stderr,"Need outfile name\n");
        return -1;
    }

    osg::ref_ptr<osg::GraphicsContext> pbuffer;

    osg::State *state= new osg::State;

    bool reimage=false;
    std::string tex_cache_dir;
    texcache_t cache;
    osg::Vec4 zrange;
    arguments.read("--zrange",zrange[0],zrange[1]);
    bool useAtlas=arguments.read("--atlas");
    float tex_margin=0.0;
float bbox_margin=0.0;
    if(    arguments.read("--tex-margin",tex_margin)){
        printf("Tex Margin %f\n",tex_margin);
    }
    if(    arguments.read("--bbox-margin",bbox_margin)){
        printf("BBox Margin %f\n",bbox_margin);
    }

    int row,col,numRows,numCols,width,height;
    bool imageNode=arguments.read("--imageNode",row,col,numRows,numCols,width,height);
    bool untex=arguments.read("--untex");
    bool debug=arguments.read("--debug-shader");
    bool depth=arguments.read("--depth");
    int size;

    if(arguments.read("--tex_cache",tex_cache_dir,size)){
        reimage=true;
        cache.push_back(std::make_pair<std::string,int>(tex_cache_dir,size));
    }else{
        cache.push_back(std::make_pair<std::string,int>(tex_cache_dir,std::max(calib.camera_calibs[0].width,calib.camera_calibs[0].height)));
    }
    osg::Matrixd viewProjRead;
    double lat=0,lon=0;

    if(reimage){
        string matfile;
        if(!arguments.read("--mat",matfile)){
            fprintf(stderr,"Fail mat file\n");
            return -1;
        }
        if(!arguments.read("-lat",lat)|| !arguments.read("-lon",lon)){
            fprintf(stderr,"Can't get lat long\n");
            return -1;
        }
        std::fstream _file(matfile.c_str(),std::ios::binary|std::ios::in);
        if(!_file.good()){
            fprintf(stderr,"Can't load %s\n",matfile.c_str());
            exit(-1);
        }

        for(int i=0; i<4; i++)
            for(int j=0; j<4; j++){
                _file.read(reinterpret_cast<char*>(&(viewProjRead(i,j))),sizeof(double));
            }
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


    if(reimage){
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        traits->x = 0;
        traits->y = 0;
        traits->width = 1;
        traits->height = 1;
        traits->red = 8;
        traits->green = 8;
        traits->blue = 8;
        traits->alpha = 8;
        traits->windowDecoration = false;
        traits->pbuffer = true;
        traits->doubleBuffer = true;
        traits->sharedContext = 0;

        pbuffer = osg::GraphicsContext::createGraphicsContext(traits.get());
        // std::cout << "Buffer obj "<< pbuffer->getState()->getMaxBufferObjectPoolSize() << " tex "<<  pbuffer->getState()->getMaxBufferObjectPoolSize() <<std::endl;
        if (pbuffer.valid())
        {
            //   osg::notify(osg::INFO)<<"Pixel buffer has been created successfully."<<std::endl;
        }
        else
        {
            osg::notify(osg::INFO)<<"Pixel buffer has not been created successfully."<<std::endl;
        }
        if (pbuffer.valid())


        {
            pbuffer->realize();
            pbuffer->makeCurrent();
        }
    }

    std::string mf=argv[2];
    /* std::string sha2hash;
    int res=checkCached(mf,outfilename,sha2hash);
    if(res == -1)
        return -1;
    else if(res == 1)
        return 0;//Hash is valid
    cout <<"Computing hash\n";*/
    //Differing hash or no hash
    //int npos=mf.find("/");
    std::string bbox_file;
    if(!arguments.read("--bbfile",bbox_file)){
        fprintf(stderr,"Can't get bbox \n");
        exit(-1);
        }//std::string(mf.substr(0,npos)+"/bbox-"+mf.substr(npos+1,mf.size()-9-npos-1)+".ply.txt");
    printf("SS %s\n",bbox_file.c_str());
    TexturedSource *sourceModel=new TexturedSource(vpb::Source::MODEL,mf,bbox_file,true,false,bbox_margin);
    osgDB::Registry::instance()->setBuildKdTreesHint(osgDB::ReaderWriter::Options::BUILD_KDTREES);
      ply::VertexDataMosaic vertexData;
    osg::Node* model = vertexData.readPlyFile(sourceModel->getFileName().c_str());//osgDB::readNodeFile(sourceModel->getFileName().c_str());

    if(!model){
        fprintf(stderr,"Can't load model %s\n",sourceModel->getFileName().c_str());
        return -1;
    }
    osg::ref_ptr<osg::KdTreeBuilder>  _kdTreeBuilder = osgDB::Registry::instance()->getKdTreeBuilder()->clone();
    model->accept(*_kdTreeBuilder);
    osg::ref_ptr<osg::MatrixTransform>xform = new osg::MatrixTransform;
    xform->setDataVariance( osg::Object::STATIC );
    xform->setMatrix(rotM);
    osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv(NULL);
    xform->addChild(model);
    xform->accept(fstv);
    fstv.removeTransforms(xform);
    osg::ComputeBoundsVisitor cbbv(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN);
    model->accept(cbbv);
    osg::BoundingBox bb = cbbv.getBoundingBox();
    zrange[2]=bb.zMin();
    zrange[3]=bb.zMax();
    if (model)
    {
        vpb::SourceData* data = new vpb::SourceData(sourceModel);
        data->_model = model;
        data->_extents.expandBy(model->getBound());
        sourceModel->setSourceData(data);
        osg::Geode *geode= dynamic_cast<osg::Geode*>(model);
        if(geode && geode->getNumDrawables()){
            //addDups(geode);
            osg::Drawable *drawable = geode->getDrawable(0);
            sourceModel->_kdTree = dynamic_cast<osg::KdTree*>(drawable->getShape());
        }else{
            std::cerr << "No drawbables \n";
        }
        //  TexPyrAtlas atlasGen(cache);
        //atlasGen._useAtlas=true;
        vpb::MyDataSet *dataset=new vpb::MyDataSet(calib.camera_calibs[0],basepath,false,false,false);
        dataset->setState(state);
        dataset->_zrange=zrange;
        dataset->_useAtlas=useAtlas;
        dataset->_useBlending=true;
        dataset->_useDebugShader=debug;
        // dataset->_useDisplayLists=(!imageNode);
        dataset->_useDisplayLists=true;
        dataset->_useVBO =true;
        vpb::MyDestinationTile *tile=new vpb::MyDestinationTile(cache);
        tile->_mydataSet=dataset;
        tile->_dataSet=dataset;
        tile->_atlasGen->_useAtlas=useAtlas;
        tile->_atlasGen->_useStub=false;

        TexturingQuery *tq=new TexturingQuery(sourceModel,calib.camera_calibs[0],*tile->_atlasGen,true);

        tq->_tile=tile;
        bool projectSucess=tq->projectModel(dynamic_cast<osg::Geode*>(model),tex_margin);
        if(projectSucess){
            //  writeCached(outfilename,sha2hash,tile->texCoordIDIndexPerModel.begin()->second,tile->texCoordsPerModel.begin()->second);
            //osg::Geometry *geom = dynamic_cast< osg::Geometry*>( geode->getDrawable(0));
            // for(int f=0; f<tile->texCoordsPerModel.begin()->second.size(); f++)
            //    geom->setTexCoordArray(f,tile->texCoordsPerModel.begin()->second[f]);
            if(!reimage){
                std::ofstream f(outfilename.c_str());
                std::vector<bool> *marginFace=NULL;
                if(vertexData._texIds.valid() && vertexData._texIds->size()){
                    marginFace= new std::vector<bool>( vertexData._vertices->size(),false);
                    if( vertexData._vertices->size() !=  vertexData._texIds->size()*3){
                        fprintf(stderr,"%d should be 3 time %d\n", vertexData._vertices->size(),  vertexData._texIds->size());
                        exit(-1);
                    }
                    for(int i=0; i< (int)vertexData._triangles->size(); i+=3)
                        for(int j=0;j<3; j++)
                            marginFace->at(vertexData._triangles->at(i+j))= (vertexData._texIds->at(i/3).y() == -999);
                }

                PLYWriterNodeVisitor nv(f,tile->texCoordIDIndexPerModel.begin()->second,&(tile->texCoordsPerModel.begin()->second),"",marginFace,vertexData._colors);
                model->accept(nv);
            }else{
                // map<SpatialIndex::id_type,int> allIds=calcAllIds(tile->texCoordIDIndexPerModel.begin()->second);
                // tq->addImagesToAtlasGen(allIds);
                tile->_models = new vpb::DestinationData(NULL);
                tile->_models->_models.push_back(model);
                osg::ref_ptr<osg::Node> node=tile->createScene();
                osg::ref_ptr<osg::MatrixTransform>xform = new osg::MatrixTransform;
                xform->setDataVariance( osg::Object::STATIC );
                xform->setMatrix(inverseM);
                xform->addChild(node);
                osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv(NULL);
                xform->accept(fstv);
                fstv.removeTransforms(xform);
                if(imageNode){
                    osg::Matrixd view,proj;


               /*     std::stringstream os2;
                    os2<< "viewproj.mat";

                    std::fstream _file(os2.str().c_str(),std::ios::binary|std::ios::in);
                    for(int i=0; i<4; i++)
                        for(int j=0; j<4; j++)
                            _file.read(reinterpret_cast<char*>(&(view(i,j))),sizeof(double));
                    for(int i=0; i<4; i++)
                        for(int j=0; j<4; j++)
                            _file.read(reinterpret_cast<char*>(&(proj(i,j))),sizeof(double));
                    _file.close();*/

                    imageNodeGL(xform.get(),numRows,numCols,width,height,row,col,untex,depth,viewProjRead,osg::Vec2(lat,lon),"png");

                }
                if(!imageNode){
                    osgDB::ReaderWriter::Options* options = new osgDB::ReaderWriter::Options;
                    printf("AAAA %s\n",options->getOptionString().c_str());
                    // options->setOptionString("compressed=1 noTexturesInIVEFile=1 noLoadExternalReferenceFiles=1 useOriginalExternalReferences=1");
                    osgDB::Registry::instance()->setOptions(options);

                    osgDB::writeNodeFile(*xform,osgDB::getNameLessExtension(outfilename).append(".ive"));
                }
                /* osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv(NULL);
                xform->accept(fstv);
                fstv.removeTransforms(xform);
*/

                /*   osgDB::ReaderWriter::WriteResult wr;

                osgDB::ReaderWriter* rw =  osgDB::Registry::instance()->getReaderWriterForExtension("ive");
                wr = rw->writeNode( *dynamic_cast<osg::Geode*>(model), osgDB::getNameLessExtension(outfilename).append(".ive"), new osgDB::Options("compressed 1") );
            //   osgDB::writeNodeFile(*model,osgDB::getNameLessExtension(outfilename).append(".ivez"),osgDB::Registry::instance()->getOptions());
                       //osgDB::Registry::instance()->writeNode(*node,osgDB::getNameLessExtension(outfilename).append(".ive"), osgDB::Registry::instance()->getOptions() );*/
                /*  std::ofstream f(outfilename.c_str());
                PLYWriterNodeVisitor nv(f,tile->texCoordIDIndexPerModel.begin()->second,&(tile->texCoordsPerModel.begin()->second));
                model->accept(nv);*/
                //    if (!wr.success() )     OSG_NOTIFY( osg::WARN ) << "ERROR: Savefailed: " << wr.message() << std::endl;
            }

        }else
            cerr << "Failed to project\n";
        delete tq;
    }else
        cerr << "Failed to open "<<sourceModel->getFileName() <<endl;

}
