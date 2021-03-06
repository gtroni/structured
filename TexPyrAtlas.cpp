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

#include "TexPyrAtlas.h"
#include <osgDB/ReadFile>
#include <string.h>
#include <assert.h>
#include "Clipper.h"
#include <osg/io_utils>
#include <iostream>
#include "MSC.hpp"
#include <ostream>
#include <sstream>
#include <osgDB/FileNameUtils>
using namespace std;
TexPyrAtlas::TexPyrAtlas(texcache_t imgdir):_imgdir(imgdir)
{
    setMaximumAtlasSize(8192,8192);
    setMargin(5);
    _downsampleSizes.clear();
    _downsampleSizes.push_back(512);
    _downsampleSizes.push_back(128);
    _downsampleSizes.push_back(32);
    _state = new osg::State;
    _useTextureArray=false;
    _useAtlas=true;
    _useStub=true;
    srand(getpid());
    _sets=NULL;
}
int TexPyrAtlas::getMaxNumImagesPerAtlas(void)

{
    if(!_imgdir.size())
        return 0;
    double area=pow((double)_imgdir[0].second + 2*_margin,2);
    //printf("%f %d\n",area,_imgdir[0].second);
    return (int)floor((_maximumAtlasWidth*_maximumAtlasHeight)/area);
}
struct Candidate;

typedef multimap<int, Candidate *> OpenList;

struct Candidate {
  set<int> elements;
  int index;
  OpenList::iterator pos;
  Candidate() {}
  Candidate(const set<int> &elem, int i) : elements(elem), index(i) {}
};

vector<int> set_cover(const vector<vector<int> > &input) {
  /*
    "input" encodes a system of sets of integers, S = {S_1, ... S_n}.
    The returned vector contains a list of unique indices i1, ..., ik such
    that T = {S_i1, ..., S_ik} is a minimal subset of S satisfying
    S_i1 \cup ... \cup S_ik = S_1 \cup ... \cup S_n.
    The list is returned in the order of creation, i.e. sets which contribute
    much to the cover appear near the start.

    This uses a greedy algorithm: At every stage, add a set containing a
    maximal number of elements not contained in the covering chosen so far.
   */


  OpenList openList;
  vector<Candidate> candidate;
  multimap<int, Candidate *> itemToCandidates;
  for(int i = 0; i < (int)input.size(); i++)
    candidate.push_back(Candidate(set<int>(input[i].begin(), input[i].end()), i));

  // Pointers into the candidate vector won't change from here onwards.
  for(int i = 0; i < (int)candidate.size(); i++) {
    candidate[i].pos = openList.insert(make_pair(-input[i].size(), &candidate[i]));
    for(int j = 0; j < (int)input[i].size(); j++)
      itemToCandidates.insert(make_pair(input[i][j], &candidate[i]));
  }

  vector<int> result;
  while(!openList.empty()) {
    OpenList::iterator top = openList.begin();
    if(top->first == 0)
      break;
    Candidate *selectedCand = top->second;
    openList.erase(top);
    result.push_back(selectedCand->index);
    set<int>::iterator curr, end = selectedCand->elements.end();
    for(curr = selectedCand->elements.begin(); curr != end; ++curr) {
      int item = *curr;
      typedef multimap<int, Candidate *>::const_iterator CandIt;
      pair<CandIt, CandIt> candRange = itemToCandidates.equal_range(item);
      CandIt currCand, lastCand = candRange.second;
      for(currCand = candRange.first; currCand != lastCand; ++currCand) {
        Candidate *cand = currCand->second;
        if(cand != selectedCand) {
          cand->elements.erase(item);
          openList.erase(cand->pos);
          cand->pos = openList.insert(make_pair(-cand->elements.size(), cand));
        }
      }
    }
  }

  return result;
}
#if 0
unsigned int max_tree_depth=100;

class AtlasBounds
{
public:
    AtlasBounds( ){};
    AtlasBounds( const osg::Vec3Array *pts,const osg::Vec4Array *camIdx );

    void set( double min_x, double max_x, double min_y, double max_y );

    double area( void ) const;

    double min_x;
    double max_x;
    double min_y;
    double max_y;
    std::set<long> ptset;
    std::vector<int> idxset;

};
double AtlasBounds::area( void ) const
{
    return (max_x-min_x)*(max_y-min_y);
}

AtlasBounds::AtlasBounds( const osg::Vec3Array *pts,const osg::Vec4Array *camIdx )
{
    for( unsigned int i=0; i<pts->size( ) ; i++ )
    {
        idxset.push_back(i);

        double x1 = pts->at(i)[0];
        double x2 = pts->at(i)[0];
        double y1 = pts->at(i)[1];
        double y2 = pts->at(i)[1];

        if( i==0 || x1 < min_x )
            min_x = x1;
        if( i==0 || x2 > max_x )
            max_x = x2;
        if( i==0 || y1 < min_y )
            min_y = y1;
        if( i==0 || y2 > max_y )
            max_y = y2;
        for(int j=0; j<maxNumTC; j++){
            long id=(int)camIdx->at(i)[j];

            ptset.insert(id);
        }
    }
}

void AtlasBounds::set( double min_x, double max_x, double min_y, double max_y )
{
    this->min_x = min_x;
    this->max_x = max_x;
    this->min_y = min_y;
    this->max_y = max_y;
}
#define VERBOSE 1
static void split_data(const osg::Vec3Array *pts,
                       const osg::PrimitiveSet& prset,
                       const osg::Vec4Array *blendIdx,
                       const AtlasBounds                           &bounds,
                       unsigned int                            split_axis,
                       double                                  split_value,
                       AtlasBounds                                 &bounds1,
                       AtlasBounds                                 &bounds2 )
{
    // Split the data - here we don't just consider if the center of the stereo
    // is in a cell, but if any part of the footprint is. Therefore a pose can
    // be in multiple cells.
    int numIdx=prset.getNumIndices();
    for(int i=0; i<numIdx-2; i+=3){


        if( pts->at(prset.index(i+0))[split_axis] < split_value ){
            for(int k=0; k <3; k++)
            {
                for(int j=0; j<maxNumTC; j++){
                    long id=(int)blendIdx->at(prset.index(i+k))[j];
                    bounds1.ptset.insert(id );
                    bounds1.idxset.push_back(prset.index(i+k));
                }
            }
        }

        if(pts->at(prset.index(i+0))[split_axis] >= split_value )
        {
            for(int k=0; k <3; k++)
            {
                for(int j=0; j<maxNumTC; j++){
                    long id=(int)blendIdx->at(prset.index(i+k))[j];
                    bounds2.ptset.insert(id);
                    bounds2.idxset.push_back(prset.index(i+k));

                }
            }
        }
    }


    // Create the bounds for the split data sets
    if( split_axis == POSE_INDEX_X )
    {
        bounds1.set( bounds.min_x, split_value , bounds.min_y, bounds.max_y );
        bounds2.set( split_value , bounds.max_x, bounds.min_y, bounds.max_y );
    }
    else
    {
        bounds1.set( bounds.min_x, bounds.max_x, bounds.min_y, split_value );
        bounds2.set( bounds.min_x, bounds.max_x, split_value , bounds.max_y );
    }
}


static void recursive_split_area(const osg::Vec3Array *pts,
                                 const osg::PrimitiveSet& prset,
                                 const osg::Vec4Array *blendIdx,
                                 std::vector<char> &atlasmap,
                                 const int &max_img_per_atlas,
                                 const AtlasBounds &bounds,
                                 unsigned int depth, vector< std::set<long>  > &sets)
{
    if( depth > max_tree_depth )
    {
        cerr << "ERROR - maximum recursion level reached in recursive_split" << endl;
        exit(1);
    }



    if((int)bounds.ptset.size() <= max_img_per_atlas ){
        sets.push_back(bounds.ptset);
        for(int i=0; i < (int)atlasmap.size(); i++)
            atlasmap[i]=sets.size()-1;

        return ;
    }
    unsigned int best_axis;
    double best_split_point;

    if(bounds.max_x-bounds.min_x  > bounds.max_y-bounds.min_y){
        best_axis= POSE_INDEX_X ;
        best_split_point=(bounds.max_x+bounds.min_x)/2;
    }else{
        best_axis= POSE_INDEX_Y ;
        best_split_point=(bounds.max_y+bounds.min_y)/2;
    }
    // Perform the split
    AtlasBounds bounds1, bounds2;
    split_data( pts,prset,blendIdx, bounds, best_axis, best_split_point,
                bounds1,  bounds2 );


    // Check if the subsets need to be recursively split
    if(bounds1.ptset.size()  > max_img_per_atlas&& bounds1.area() > 0.01)
    {
        recursive_split_area(pts,prset,blendIdx, atlasmap,max_img_per_atlas, bounds1, depth+1,sets );
    }
    else
    {
        if(bounds1.ptset.size() >max_img_per_atlas){
            fprintf(stderr,"Bailed out too small an area\n");
        }
        for(int i=0;i< bounds1.idxset.size(); i++){
            if(bounds1.idxset[i] >=0 && bounds1.idxset[i] < atlasmap.size())
                atlasmap[bounds1.idxset[i]]=sets.size();
            else
                printf("Failure out of bounds %d %d\n",i,bounds1.idxset[i]);
        }
        sets.push_back( bounds1.ptset );


#if VERBOSE
        printf( "Area Cell %d at depth %d area: %f %f %f %f %f, poses: %d vertex %d\n", sets.size(), depth,
                bounds1.area(), bounds1.min_x,bounds1.max_x,bounds1.min_y,bounds1.max_y,bounds1.ptset.size() ,bounds1.idxset.size());
        /*     for(int i=0; i < bounds1.idxset.size(); i++)
            std::cout<< pts->at(bounds1.idxset[i]) << "\n";
        std::cout<<"!!!!!!!!!!!\n";
*/
        for(std::set<long>::iterator itr=bounds1.ptset.begin();
        itr!=bounds1.ptset.end();
        ++itr)
            printf("%d ",*itr);
        printf("************\n");

#endif
    }


    if(bounds2.ptset.size()  > max_img_per_atlas && bounds2.area() > 0.01)
    {
        recursive_split_area( pts,prset,blendIdx,atlasmap,max_img_per_atlas, bounds2, depth+1,sets);
    }
    else
    {
        if(bounds2.ptset.size() >max_img_per_atlas){
            fprintf(stderr,"Bailed out too small an area\n");
        }
        for(int i=0;i< bounds2.idxset.size(); i++){
            if(bounds2.idxset[i] >=0 && bounds2.idxset[i] < atlasmap.size())
                atlasmap[bounds2.idxset[i]]=sets.size();
            else
                printf("Failure out of bounds %d %d\n",i,bounds2.idxset[i]);
        }
        sets.push_back( bounds2.ptset );

#if VERBOSE
        printf( "Area Cell %d at depth %d area: %f %f %f %f %f, poses: %d vertex %d\n", sets.size(), depth,
                bounds2.area(), bounds2.min_x,bounds2.max_x,bounds2.min_y,bounds2.max_y,bounds2.ptset.size() ,bounds2.idxset.size());
        /*  for(int i=0; i < bounds2.idxset.size(); i++)
            std::cout<< pts->at(bounds2.idxset[i]) << "\n";
        std::cout<<"!!!!!!!!!!!\n";
      */  for(std::set<long>::iterator itr=bounds2.ptset.begin();
          itr!=bounds2.ptset.end();
          ++itr)
              printf("%d ",*itr);
          printf("************\n");

#endif
      }

}
#endif
class is_same
{
public:
    bool operator() (pair<int,set<SpatialIndex::id_type> > &first, pair<int,set<SpatialIndex::id_type> > &second)
    {
        if(first.second.size() ==0 || second.second.size() == 0)
            return false;
        if(first.second == second.second)
            return true;
        else{
            if(includes(first.second.begin(),first.second.end(),second.second.begin(),second.second.end()))
                return true;
            // else
            //   if(includes(second.second.begin(),second.second.end(),first.second.begin(),first.second.end()))
            //     return true;
            else
                return false;

        }
    }
};

std::vector< std::set<SpatialIndex::id_type>  >  calc_atlases(const osg::Vec3Array *pts,
                                             const osg::PrimitiveSet& prset,
                                             const osg::Vec4Array *blendIdx,
                                             std::vector<char> &atlasmap,
                                             int max_img_per_atlas )
{
    if(max_img_per_atlas<=12){
        printf("Can't ensure  less then 12 images per area due to 4*3 images in triangles\n");
    }
    vector< std::set<SpatialIndex::id_type>  >sets;

    printf("Max Images per Atlas %d\n",max_img_per_atlas);
    //bool area_split=false;
#if 0
    if(area_split){


        // Calculate the bounds of the stereo data
        AtlasBounds bounds( pts,blendIdx );
#if VERBOSE
        printf( "Bounds: %f %f %f %f\n", bounds.min_x, bounds.max_x ,
                bounds.min_y, bounds.max_y );
#endif



        // Perform binary division until termination criteria

        recursive_split_area( pts,prset,blendIdx,atlasmap,max_img_per_atlas, bounds,0, sets );
    }else
#endif
    {
        set<SpatialIndex::id_type> imgcount;

        list<pair<int,set<SpatialIndex::id_type> >  > list_sets;
        for(int i=0;i <(int)blendIdx->size(); i++){
            pair<int,set<SpatialIndex::id_type> > iForV;
            iForV.first=i;
            for(int j=0; j<4; j++){
                long idx=(int)blendIdx->at(i)[j];
                if(idx >= 0)
                    iForV.second.insert(idx);
                imgcount.insert(idx);
            }
            if(iForV.second.size())
                list_sets.push_back(iForV);
        }
        printf("Pre unique sets %d\n",(int)list_sets.size());

        list_sets.unique(is_same());
        printf("Unique sets %d\n",(int)list_sets.size());
        vector<vector<int> > input;

        list<pair<int,set<SpatialIndex::id_type> >  >::iterator it=list_sets.begin();
        for(; it!= list_sets.end(); it++){
            vector<int> set1(it->second.begin(),it->second.end());
            input.push_back(set1);
        }

        vector<int> result = set_cover(input);
        set<SpatialIndex::id_type> currSet;
        currSet.insert(-1);
        list< set<SpatialIndex::id_type>  > sets_tmp;

        sets_tmp.push_back(currSet);

        bool opt=false;
        if(opt){
            for(int i = 0; i < (int)result.size(); i++) {
                if((int)sets.back().size()+maxNumTC > max_img_per_atlas){
                    set<SpatialIndex::id_type> currSet;
                    currSet.insert(-1);
                    sets.push_back(currSet);
                }
                vector<int> &vec = input[result[i]];
                for(int j = 0; j < (int)vec.size(); j++){
                    sets.back().insert(vec[j]);
                    //  cout << " " << vec[j];
                }
            }
        }else{
            it=list_sets.begin();
            for(; it!= list_sets.end(); it++){
                vector<int> set1(it->second.begin(),it->second.end());
                if((int)set1.size()+ (int)sets_tmp.back().size() > max_img_per_atlas){
                    set<SpatialIndex::id_type> currSet;
                    currSet.insert(-1);
                    sets_tmp.push_back(currSet);
                }
                for(int j = 0; j < (int)set1.size(); j++){
                    sets_tmp.back().insert(set1[j]);
                    //  cout << " " << vec[j];
                }
                // cout << endl;
            }
            sets_tmp.unique();
            sets.insert(sets.begin(),sets_tmp.begin(),sets_tmp.end());
        }
        int numIdx=prset.getNumIndices();
        for(int i=0; i<numIdx-2; i+=3){
            std::set<SpatialIndex::id_type> id_per_vert;
            for(int k=0; k <3; k++){
                for(int j=0; j<maxNumTC; j++){
                    SpatialIndex::id_type id=(int)blendIdx->at(prset.index(i+k))[j];
                    if(id>=0)
                        id_per_vert.insert(id);
                }

            }
            bool setface=false;
            for(int s=0; s< (int)sets.size(); s++){
                std::set<SpatialIndex::id_type>:: iterator iter=sets[s].begin();
                if(includes(sets[s].begin(),sets[s].end(),id_per_vert.begin(),id_per_vert.end())){
                    for(int k=0; k <3; k++)
                        atlasmap[prset.index(i+k)]=s;
                    setface=true;
                    break;
                }
            }
            if(!setface){
                fprintf(stderr,"Can't find atlas for index %d\n",i);
                std::set<SpatialIndex::id_type>:: iterator iter=id_per_vert.begin();
                for(; iter!=id_per_vert.end(); iter++)
                    std::cerr<<*iter<<" ";
                std::cerr<<"!!!!!\n";

                exit(-1);
            }
        }

       /* int pid=getpid();
        char aa[1024];sprintf(aa,"numatlas-%d.txt",pid);
        FILE *fp=fopen(aa,"w");
        fprintf(fp,"%d\n",sets.size());
        fclose(fp);
*/
        printf("Size of list sets %d %d %d\n",(int)list_sets.size(),(int)result.size(),(int)sets.size());
    }
    return sets;
}
void TexPyrAtlas::computeImageNumberToAtlasMap(void){
    for(int i=0; i < (int)_atlasList.size(); i++){
        for(int j=0; j< (int)_atlasList[i]->_sourceList.size(); j++)
            if(_sourceToId.count(_atlasList[i]->_sourceList[j]))
                _idToAtlas[_sourceToId[_atlasList[i]->_sourceList[j]]]=i;
    }
}

int TexPyrAtlas::getAtlasId(id_type id){
    if(id < 0 )
        return id;

    if(!_useAtlas)
        return id;
    if(_idToAtlas.count(id))
        return _idToAtlas[id];
    return -1;
}
osg::Matrix TexPyrAtlas::getTextureMatrixByID(id_type id){
    osg::Matrix matrix;
    if(_idToSource.count(id))
        if( computeTextureMatrixFreedImage(matrix,_idToSource[id]))
            return matrix;
    //   return _idToSource[id]->computeTextureMatrix(); This was using images we had freed

    return osg::Matrix::identity();
}

bool TexPyrAtlas::getTextureMatrixByIDAtlas(osg::Matrix &matrix,id_type id,char atlas){
    if(atlas >=0 && _idToSourceAtlas[atlas].count(id)){
        if(computeTextureMatrixFreedImage(matrix,_idToSourceAtlas[atlas][id]))     //   return _idToSource[id]->computeTextureMatrix(); This was using images we had freed
            return true;
    }
    matrix= osg::Matrix::identity();
    return false;
}
bool TexPyrAtlas::computeTextureMatrixFreedImage(osg::Matrix &matrix,Source *s)
{
    if (!s->_atlas){
        cerr<< "no valid atlas\n";
        matrix= osg::Matrix();
        return false;
    }
    double image_t,image_s;
    if(!_sourceToSize.count(s)){
        matrix= osg::Matrix();
        cerr<< "no source stored already\n";
        return false;
    }
    image_s=_sourceToSize[s][0];
    image_t=_sourceToSize[s][1];

    if (!(s->_atlas->_image)){
        cerr<< "no atlas image";

        matrix= osg::Matrix();
        return false;
    }
    matrix= osg::Matrix::scale(float(image_s)/float(s->_atlas->_image->s()), float(image_t)/float(s->_atlas->_image->t()), 1.0)*
            osg::Matrix::translate(float(s->_x)/float(s->_atlas->_image->s()), float(s->_y)/float(s->_atlas->_image->t()), 0.0);
    return true;
}
void TexPyrAtlas::addSources(std::vector<std::pair<id_type ,std::string> > imageList){
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_imageListMutex);

    for(int i=0; i< (int)imageList.size(); i++){

        if(_totalImageList.count(imageList[i].first) == 0 )
            _totalImageList.insert(imageList[i]);
    }
}
osg::ref_ptr<osg::Image> TexPyrAtlas::getImage(int index,int sizeIndex){
    osg::ref_ptr<osg::Image> img;
    if(!_useAtlas){
        if(index >= 0 && index < (int)_images.size() && _images[index].valid()){
            resizeImage(_images[index].get(),_downsampleSizes[sizeIndex],_downsampleSizes[sizeIndex],img);
        }

        return img;
    }else{
        return getAtlasByNumber(index);
    }
    /*  if(index >= 0 && index < (int)_images.size() && _images[index].valid())
        return _tb.getImageAtlas(_images[index]);
    else
        return osg::ref_ptr<osg::Image> ();*/
}
osg::ref_ptr<osg::Image> TexPyrAtlas::getImageFullorStub(string fname,int size){
    if(_useStub){
        osg::ref_ptr<osg::Image> img= new osg::Image();
        img->setFileName(fname);
        img->setImage(size,size,3,GL_RGB, GL_RGB, GL_UNSIGNED_BYTE,NULL,osg::Image::NO_DELETE);
        img->setWriteHint(osg::Image::EXTERNAL_FILE);
        return img;
    }
    return osgDB::readImageFile(osgDB::getNameLessExtension(fname)+".png");
}
string getUUID(void){
    char strUuid[1024];

    sprintf(strUuid, "%x%x-%x-%x-%x-%x%x%x",
            rand(), rand(),                 // Generates a 64-bit Hex number
            rand(),                         // Generates a 32-bit Hex number
            ((rand() & 0x0fff) | 0x4000),   // Generates a 32-bit Hex number of the form 4xxx (4 indicates the UUID version)
            rand() % 0x3fff + 0x8000,       // Generates a 32-bit Hex number in the range [0x8000, 0xbfff]
            rand(), rand(), rand());        // Generates a 96-bit Hex number
    return string(strUuid);
}

void TexPyrAtlas::loadTextureFiles(int size){
    std::vector<osg::ref_ptr<osg::Image> > loc_images;
    if( _totalImageList.size() == 0)
        return;
    string closestDir;
    if(!getClosestDir(closestDir,size)){
        cerr << "Can't find any dir close to size " << size<<endl;
        exit(-1);
    }
    std::map<id_type,string>::const_iterator end = _totalImageList.end();
    for (std::map<id_type,string>::const_iterator it = _totalImageList.begin(); it != end; ++it){
        string fname=closestDir+"/"+it->second;
        osg::ref_ptr<osg::Image> img=getImageFullorStub(fname,size);
        if(!img.valid()){
            fprintf(stderr,"Can't open image %s if IDAT error CRC is printed above delete the offending cached image and rerun there is a corrupt image in the cache.\n",fname.c_str());
            exit(-1);
        }
        osg::ref_ptr<osg::Image> tmp=NULL;
        if(img->s() == size && img->t() == size)
            tmp=img;
        else
            resizeImage(img,size,size,tmp);
        loc_images.push_back(tmp);
        if(loc_images.back().valid()){
            if(!_useAtlas) {
                _allIDs[it->first]=_images.size();
                _images.push_back(loc_images.back());
                if(!_useStub)
                    _images.back()->setFileName(getUUID());

            }else{
                //texture->setImage(_images[i]);
                /*   texture->setTextureSize(_downsampleSizes[0],_downsampleSizes[0]);
                bool resizePowerOfTwo=true;
                osg::NotifySeverity saved_ns=osg::getNotifyLevel();
                // osg::setNotifyLevel(osg::FATAL);
                vpb::generateMipMap(*_state,*texture,resizePowerOfTwo,vpb::BuildOptions::GL_DRIVER);
                // osg::setNotifyLevel(saved_ns);*/
                if (!getSource(loc_images.back())) {
                    Source *s=new Source(loc_images.back());
                    _sourceList.push_back(s);
                    _sourceToId[s]=it->first;
                    _idToSource[it->first]=s;
                }
            }
        }else{
            cout << it->second << " not found or couldn't be loaded"<<std::endl;
        }
    }


    if(_useAtlas){
        buildAtlas();
        computeImageNumberToAtlasMap();
        for(int i=0; i < (int)getNumAtlases(); i++){
            _images.push_back(getAtlasByNumber(i));
            _images.back()->setFileName(getUUID()+".png");
        }
        //Free ref created by source new above which leaks after copy from sources
        // for(int i=0; i < (int)loc_images.size(); i++){
        //    loc_images[i]->unref();
        //}

    }
    /* for(int i=0; i < loc_images.size(); i++){
            OSG_ALWAYS << loc_images[i]->referenceCount() << endl;
        }*/
}
osg::ref_ptr<osg::Texture> TexPyrAtlas::getTexture(int index,int sizeIndex){

    if(index >= 0 && index < (int)_images.size() && _images[index].valid())
        return getTextureAtlas(_images[index]);
    else
        return osg::ref_ptr<osg::Texture> ();
}

bool
        TexPyrAtlas::resizeImage(const osg::Image* input,
                                 unsigned int out_s, unsigned int out_t,
                                 osg::ref_ptr<osg::Image>& output,
                                 unsigned int mipmapLevel )
{
    if ( !input && out_s == 0 && out_t == 0 )
        return false;

    GLenum pf = input->getPixelFormat();

    //if ( pf != GL_RGBA && pf != GL_RGB && pf != GL_LUMINANCE && pf != GL_RED && pf != GL_LUMINANCE_ALPHA )
    //{
    //    OE_WARN << LC << "resizeImage: unsupported pixel format " << std::hex << pf << std::endl;
    //    return 0L;
    //}

    unsigned int in_s = input->s();
    unsigned int in_t = input->t();

    if ( !output.valid() )
    {
        output = new osg::Image();
        output->allocateImage( out_s, out_t, 1, pf, input->getDataType(), input->getPacking() );
        assert(output.valid() && output->data());
    }
    output->setInternalTextureFormat( input->getInternalTextureFormat() );

    if ( in_s == out_s && in_t == out_t && mipmapLevel == 0 )
    {
        memcpy( output->getMipmapData(mipmapLevel), input->data(), input->getTotalSizeInBytes() );
    }
    else
    {
        //float s_ratio = (float)in_s/(float)out_s;
        //float t_ratio = (float)in_t/(float)out_t;
        unsigned int pixel_size_bytes = input->getRowSizeInBytes() / in_s;

        unsigned char* dataOffset = output->getMipmapData(mipmapLevel);
        unsigned int   dataRowSizeBytes = output->getRowSizeInBytes() >> mipmapLevel;

        for( unsigned int output_row=0; output_row < out_t; output_row++ )
        {
            // get an appropriate input row
            float output_row_ratio = (float)output_row/(float)out_t;
            int input_row = (unsigned int)( output_row_ratio * (float)in_t );
            if ( input_row >= input->t() ) input_row = in_t-1;
            else if ( input_row < 0 ) input_row = 0;

            for(unsigned int output_col = 0; output_col < out_s; output_col++ )
            {
                float output_col_ratio = (float)output_col/(float)out_s;
                int input_col = (unsigned int)( output_col_ratio * (float)in_s );
                if ( input_col >= (int)in_s ) input_col = in_s-1;
                else if ( input_row < 0 ) input_row = 0;

                unsigned char* outaddr =
                        dataOffset +
                        (output_col*output->getPixelSizeInBits())/8+output_row*dataRowSizeBytes;

                memcpy(
                        outaddr,
                        input->data( input_col, input_row ),
                        pixel_size_bytes );
            }
        }
    }

    return true;
}


void TexPyrAtlas::buildAtlas(   )
{
    //printf("Number of sources %d\n",(int)_sourceList.size());
    // assign the source to the atlas
    _atlasList.clear();

    if(_sets){
        _idToSourceAtlas.resize(_sets->size());
        for(int i=0; i < (int)_sets->size(); i++){

            osg::ref_ptr<Atlas> atlas = new Atlas(_maximumAtlasWidth,_maximumAtlasHeight,_margin);
            if(_atlasList.size() && _useTextureArray){
                atlas->_width=_maximumAtlasWidth;
                atlas->_height=_maximumAtlasHeight;
            }
            _atlasList.push_back(atlas.get());
            for(std::set<id_type>::iterator it=_sets->at(i).begin(); it!=_sets->at(i).end(); it++){
                id_type idx= *it;
                if(idx>=0){
                    Source* source = new Source(_idToSource[idx]->_image);
                    if(source){
                        _idToSourceAtlas[i][idx]=source;
                        _sourceToSize[source]=osg::Vec2(_idToSource[idx]->_image->s(),_idToSource[idx]->_image->t());
                        if(!atlas->addSource(source)){
                            fprintf(stderr,"Failed to add source doesn't fit %d %d \n",_idToSource[idx]->_image->s(),_idToSource[idx]->_image->t());
                        }
                    }
                    else
                        fprintf(stderr,"Failed to find source for id %d\n",(int)idx);

                }

            }
        }
    }else{
        for(SourceList::iterator sitr = _sourceList.begin();
        sitr != _sourceList.end();
        ++sitr)
        {
            Source* source = sitr->get();
            _sourceToSize[source]=osg::Vec2(source->_image->s(),source->_image->t());

            if (source->suitableForAtlas(_maximumAtlasWidth,_maximumAtlasHeight,_margin))
            {
                bool addedSourceToAtlas = false;
                for(AtlasList::iterator aitr = _atlasList.begin();
                aitr != _atlasList.end() && !addedSourceToAtlas;
                ++aitr)
                {
                    osg::notify(osg::INFO)<<"checking source "<<source->_image->getFileName()<<" to see it it'll fit in atlas "<<aitr->get()<<std::endl;
                    if ((*aitr)->doesSourceFit(source))
                    {
                        addedSourceToAtlas = true;
                        (*aitr)->addSource(source);
                    }
                    else
                    {
                           osg::notify(osg::INFO)<<"source "<<source->_image->getFileName()<<" does not fit in atlas "<<aitr->get()<<std::endl;
                    }
                }

                if (!addedSourceToAtlas)
                {
                    osg::notify(osg::INFO)<<"creating new Atlas for "<<source->_image->getFileName()<<std::endl;

                    osg::ref_ptr<Atlas> atlas = new Atlas(_maximumAtlasWidth,_maximumAtlasHeight,_margin);
                    if(_atlasList.size() && _useTextureArray){
                        atlas->_width=_maximumAtlasWidth;
                        atlas->_height=_maximumAtlasHeight;
                    }
                    _atlasList.push_back(atlas.get());

                    atlas->addSource(source);
                }
            }
        }

    }

    // build the atlas which are suitable for use, and discard the rest.
    AtlasList activeAtlasList;
    for(AtlasList::iterator aitr = _atlasList.begin();
    aitr != _atlasList.end();
    ++aitr)
    {
        Atlas* atlas = aitr->get();

        /*   if (atlas->_sourceList.size()==1)
        {
            // no point building an atlas with only one entry
            // so disconnect the source.
            Source* source = atlas->_sourceList[0].get();
            source->_atlas = 0;
            atlas->_sourceList.clear();
        }*/

        if (!(atlas->_sourceList.empty()))
        {
            std::stringstream ostr;
            ostr<<"atlas_"<<activeAtlasList.size()<<".rgb";
            atlas->_image->setFileName(ostr.str());

            activeAtlasList.push_back(atlas);
            atlas->clampToNearestPowerOfTwoSize();
            atlas->copySources();
            /*    for(SourceList::iterator itr = atlas->_sourceList.begin();
            itr !=atlas->_sourceList.end();
            ++itr)
            {
                Source* source = itr->get();
                // printf("source %d\n",source->_image.)
                //   cout << "refs: "<<source->_image->referenceCount()<<std::endl;
                //   while(source->_image->referenceCount())
                    //source->_image->unref();
                // osg::Image *img=(osg::Image *)(source->_image.get());
                //  img->setImage(source->_image->s(),source->_image->t(),1,GL_RGBA,GL_RGBA,GL_UNSIGNED_BYTE,NULL,osg::Image::NO_DELETE);
                for(int i=0; i < (int)loc_images.size(); i++){
                    if(source->_image->data() == loc_images[i]->data()){
                        printf("Unrefed %s\n",loc_images[i]->getFileName().c_str());
                        if(loc_images[i]->getAllocationMode()==osg::Image::USE_MALLOC_FREE){
                            free(loc_images[i]->data());
                            loc_images[i]->setAllocationMode(osg::Image::NO_DELETE);
                        }    else if(loc_images[i]->getAllocationMode()==osg::Image::USE_NEW_DELETE){
                            delete loc_images[i]->data();
                            loc_images[i]->setAllocationMode(osg::Image::NO_DELETE);
                        }
                        //while(loc_images[i]->referenceCount())
                        //  loc_images[i]->unref();

                    }
                }
            }
            printf("unref complete\n");*/

        }
    }

    // keep only the active atlas'
    _atlasList.swap(activeAtlasList);

}


