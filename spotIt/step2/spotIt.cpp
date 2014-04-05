/********************************************************************************/
//
//      author: koshy george, kgeorge2@gmail.com, copyright 2014
//      please read license.txt provided
//      free-bsd-license
/********************************************************************************/
#include <iostream>
#include <numeric>

#include <ctime>
#include <cmath>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>

#include "spotIt.hpp"
#include "kgUtils.hpp"
#include "kMeansClustering.hpp"

using namespace std;
using namespace cv;


bool SpotIt::processCircle(
                           cv::Point &circleCenter,
                           float circleRadius,
                           Mat &inputImage,
                           Mat &roiOutputImage
                           ) {
    KG_DBGOUT( cout << "  inputImage type " << inputImage.type() << "  inputImage depth " << inputImage.depth() << endl );
    
    vector<Vec4i> hierarchy;
    
    //find an roi in input image
    
    //make roi slightly bigger than the circle specified
    int roiDim = circleRadius * 2.2;
    //roiOrigin
    Point roiOrigin(Kg::max(circleCenter.x - roiDim/2, 0) , Kg::max(circleCenter.y - roiDim/2, 0));
    //roi width, height
    int adjustedRoiWidth = Kg::min( (int)roiDim  , inputImage.cols - roiOrigin.x);
    int adjustedRoiHeight = Kg::min( (int)roiDim  , inputImage.rows - roiOrigin.y);
    Point circleCenterInRoi = circleCenter - roiOrigin;
    //reference to part of inputImage that covers roi
    Mat tempImage = inputImage(Rect(roiOrigin.x, roiOrigin.y, adjustedRoiWidth, adjustedRoiHeight));
    
    
    //inout image processing
    
    Mat  roiGrayImage, roiHsvImage;
    //copy to
    blur(tempImage, tempImage, cv::Size(3,3));
    cvtColor(tempImage, roiGrayImage, CV_BGR2GRAY);
    cvtColor(tempImage, roiHsvImage, CV_BGR2HSV);
    vector<Mat> roiHsvComponents;
    split(roiHsvImage, roiHsvComponents);
    
    //Canny
    double hysterisisRatio =  3.0;
    int lowThreshold = 70;
    int kernelSize = 3;
    Canny(roiGrayImage, roiGrayImage, lowThreshold, hysterisisRatio * lowThreshold, kernelSize );
    
    
    //findContours
    vector<vector<Point> > contours;
    findContours(
                 roiGrayImage,
                 contours,
                 hierarchy,
                 CV_RETR_EXTERNAL,
                 CV_CHAIN_APPROX_TC89_L1
                 //,roiOrigin
                 );
    
    //only process contours that fall within specified circle
    const double circleRadiusSq = circleRadius * circleRadius;
    vector<bool> shouldProcessContour(contours.size(), false);
    int nContoursGood = 0;
    for(int i=0; i < contours.size(); ++i) {
        vector<Point> &contour = contours[i];
        //number of points in the contour that falls within the circle
        int numPointsInCircle = 0;
        for(int j=0; j < contour.size(); ++j) {
            Point pRelativeToCircleCenter = contour[j] - circleCenterInRoi;
            double distSqOfPointToCenter = pRelativeToCircleCenter.x * pRelativeToCircleCenter.x  + pRelativeToCircleCenter.y  * pRelativeToCircleCenter.y;
            if( ( circleRadiusSq - distSqOfPointToCenter )  > 10 ) {
                ++numPointsInCircle;
            }
        }
        double percentageNumPointsInCircle = (double)numPointsInCircle * 100.0/ contour.size();
        //if 90% of pointsof contour falls within the circle,
        //then this is a contour that need be further processed
        if( percentageNumPointsInCircle > 90.0 ) {
            assert(contour.size() > 0);
            shouldProcessContour[i] = true;
            nContoursGood++;
        }
    }
    if(nContoursGood <= 0) {
        //if no contours were produced
        //return false
        return false;
    }
    
    //cluster contours based on KMeans
    int numClusters=0;
    vector<int> clusterAssignments;
    float sse = numeric_limits<float>::max();
    cluster(
            roiHsvComponents[0], //hue component of roi
            contours,           //output from findContours
            shouldProcessContour, //flag whether each contour need be processed
            clusterAssignments, //output cluster assignments
            numClusters,  //number of clusters,
            sse //sum of squared error
            );
    
    
    //draw the output onto the output images
    roiOutputImage.create(roiGrayImage.size(), inputImage.type() );
    roiOutputImage = Scalar(0,0,0);
    drawOutput(
               contours,
               shouldProcessContour,
               clusterAssignments,
               inputImage,
               roiOutputImage);
    
    return true;
}





void SpotIt::drawOutput(
                        const vector<vector<Point>> &contours,
                        const vector<bool> &shouldProcessContour,
                        const vector<int> &clusterAssignments,
                        Mat &inputImage,
                        Mat &roiOutputImage){
    
    
    //make random colors for displaying
    static RNG rng1(12345), rng2(54321);
    static const Scalar colorMark[] = {
        Scalar(255, 255, 255),
        Scalar(255, 0, 0),
        Scalar(0, 255, 0),
        Scalar(0, 0, 255),
        Scalar(255, 255, 0),
        Scalar(255, 0, 255),
        Scalar(0, 255, 255),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) ),
        Scalar( rng1.uniform(0, 255), rng1.uniform(0,255), rng1.uniform(0,255) )
        
    };
    
    for(int i=0; i < contours.size(); ++i) {
        if(!shouldProcessContour[i]) {
            continue;
        }
        
        int iColorIdx = clusterAssignments[i];
        drawContours(
                     roiOutputImage,
                     contours,
                     i,
                     colorMark[iColorIdx]
                     );
        drawContours(
                     inputImage,
                     contours,
                     i,
                     colorMark[iColorIdx]
                     );
        
    }
}


/********************************************************************************/
//  Clustering of contour points
//
/********************************************************************************/

//We represent each contour by a representative point (ContourRepPoint) and
//cluster the representative points of all the valid clusters.
//We choose the centroid of all the points in a contour as the representative point.


struct ContourRepresentativePoint {
    Point2f meanPoint;
    Point2f variance;
    int nPointsInContour;
    cv::Scalar hsvColor;
    cv::Scalar hsvVariance;
    ContourRepresentativePoint():nPointsInContour(0),hsvVariance(0){}
    ContourRepresentativePoint & operator +(const ContourRepresentativePoint & rhs) {
        //common reciprocal term
        float reciprocal = 1.0f/( nPointsInContour + rhs. nPointsInContour);
        
        Point2f newMeanPoint =  (meanPoint * (float)nPointsInContour + rhs.meanPoint * (float)rhs.nPointsInContour )*  reciprocal;
        
        //
        Scalar newHsvColor = hsvColor;
        newHsvColor *=  (float)nPointsInContour;
        Scalar temp = rhs.hsvColor;
        temp *=  rhs.nPointsInContour;
        newHsvColor += temp;
        newHsvColor *= reciprocal;


        //calculating  new variance
        Point2f tempPoint = variance + Point2f(meanPoint.x* meanPoint.x, meanPoint.y * meanPoint.y);
        tempPoint *= (float)(nPointsInContour);
        Point2f tempPointRhs = rhs.variance + Point2f( rhs.meanPoint.x * rhs.meanPoint.x, rhs.meanPoint.y * rhs.meanPoint.y);
        tempPointRhs *= (float)(rhs.nPointsInContour);
        variance = (tempPoint + tempPointRhs) * reciprocal - Point2f(newMeanPoint.x * newMeanPoint.x, newMeanPoint.y * newMeanPoint.y);

        Scalar tempVar = hsvVariance + Scalar(hsvColor[0] * hsvColor[0],  hsvColor[1] * hsvColor[1], hsvColor[2] * hsvColor[2], hsvColor[3] * hsvColor[3] );
        tempVar *= (float)(nPointsInContour);
        Scalar tempVarRhs = rhs.hsvVariance + Scalar(rhs.hsvColor[0] * rhs.hsvColor[0],  rhs.hsvColor[1] * rhs.hsvColor[1], rhs.hsvColor[2] * rhs.hsvColor[2], rhs.hsvColor[3] * rhs.hsvColor[3] );
        tempVarRhs *= (float)(rhs.nPointsInContour);
        hsvVariance = (tempVar + tempVarRhs);
        hsvVariance *= reciprocal;
        hsvVariance -= Scalar( newHsvColor[0] * newHsvColor[0], newHsvColor[1] * newHsvColor[1], newHsvColor[2] * newHsvColor[2], newHsvColor[3] * newHsvColor[3]);

        hsvColor = newHsvColor;
        meanPoint = newMeanPoint;
        nPointsInContour += rhs.nPointsInContour;
        return *this;
    }
    
    void initializeFromContour(const vector<Point> &contour, const Mat &roiHueComponentImage) {
        
        assert(contour.size() > 0);
        Point2f sumContourPoints, sumSqContourPoints;
        Scalar sumColor(0,0,0), sumSqColor(0,0,0);
        for(int j=0; j < contour.size(); ++j ) {
            sumContourPoints += Point2f(contour[j].x, contour[j].y);
            sumSqContourPoints += Point2f(contour[j].x*contour[j].x, contour[j].y*contour[j].y);
            
            int f = static_cast<int>(roiHueComponentImage.at<uchar>(contour[j].y, contour[j].x));
            sumColor += Scalar(f, 0,0);
            sumSqColor += Scalar(f*f, 0, 0);
        }
        //mu = sigma(x)/n
        meanPoint = sumContourPoints * (1.0f/contour.size());
        //variance = sigma(x**2)/n - mu**2
        variance = sumSqContourPoints * (1.0f/contour.size()) - Point2f(meanPoint.x * meanPoint.x, meanPoint.y * meanPoint.y);
        hsvColor =  sumColor;
        hsvColor *= (1.0f/contour.size());
        hsvVariance = sumSqColor;
        hsvVariance *= (1.0f/contour.size());
        hsvVariance -= Scalar(
            hsvColor[0] * hsvColor[0],
            hsvColor[1] * hsvColor[1],
            hsvColor[2] * hsvColor[2],
            hsvColor[3] * hsvColor[3]);
        hsvVariance[1] = hsvVariance[2] = hsvVariance[3] = 1.0f;
        nPointsInContour = contour.size();
    }


    template<typename Iter>
    static void meanNormalization(Iter b, Iter e) {
        ContourRepresentativePoint cpAvg;
        cpAvg = std::accumulate(b, e, cpAvg);
        while(b != e) {
            ContourRepresentativePoint &cRepPoint = *b;
            cRepPoint.meanPoint -= cpAvg.meanPoint;
            cRepPoint.meanPoint.x /= (sqrt(cpAvg.variance.x) + 0.000001);
            cRepPoint.meanPoint.y /= (sqrt(cpAvg.variance.y) + 0.000001);
            cRepPoint.hsvColor = cpAvg.hsvColor;
            cRepPoint.hsvColor[0] /= sqrt(cpAvg.hsvVariance[0]);
            cRepPoint.hsvColor[1] /= sqrt(cpAvg.hsvVariance[1]);
            cRepPoint.hsvColor[2] /= sqrt(cpAvg.hsvVariance[2]);
            cRepPoint.hsvColor[3] /= sqrt(cpAvg.hsvVariance[3]);
            cRepPoint.variance = Point2f(1,1);
            cRepPoint.hsvVariance = Scalar(1,1,1,1);
            cRepPoint.nPointsInContour = 1;
            ++b;
        }
    }
    
};

struct ClusterItem {
    float data_[4];

    ClusterItem(){
        data_[0] = data_[1] = data_[2] = data_[3] = 0;
    }
    ClusterItem(const ClusterItem &rhs) {
        data_[0] = rhs.data_[0];
        data_[1] = rhs.data_[1];
        data_[2] = rhs.data_[2];
        data_[3] = rhs.data_[3];
    }
    ClusterItem &operator=(const ClusterItem &rhs){
        data_[0] = rhs.data_[0];
        data_[1] = rhs.data_[1];
        data_[2] = rhs.data_[2];
        data_[3] = rhs.data_[3];
        return *this;
    }
    ClusterItem &operator+(const ClusterItem &rhs) {

        data_[0] += rhs.data_[0];
        data_[1] += rhs.data_[1];
        data_[2] += rhs.data_[2];
        data_[3] += rhs.data_[3];
        return *this;
    }

    ClusterItem &operator*(float scale) {
        data_[0] *= scale;
        data_[1] *= scale;
        data_[2] += scale;
        data_[3] += scale;
        return *this;
    }

    ClusterItem &operator-(const ClusterItem &rhs) {
        data_[0] -= rhs.data_[0];
        data_[1] -= rhs.data_[1];
        data_[2] -= rhs.data_[2];
        data_[3] -= rhs.data_[3];
        return *this;
    }

    //last weignt item is the sum of all other weights
    friend float dot(const ClusterItem &lhs, const ClusterItem &rhs , const float weights[]) {
        return (weights[0] * lhs.data_[0] * rhs.data_[0] +  weights[1] * lhs.data_[1] * rhs.data_[1] +  weights[2] * lhs.data_[2] * rhs.data_[2] + weights[3] * lhs.data_[3] * rhs.data_[3])/weights[4];
    }


};


template<>
struct KMeansDataElementTraits<ClusterItem>{
    typedef ClusterItem T;
    const static float weights[];// = {1,1,9,0,11};

    static float getMinDist(float minDist, const T &c) {
        return minDist;
    }

    static T& getContribution( T &c) {
        return c;
    }

    static const T& getContribution( const T &c) {
        return c;
    }

    static int getContributingNumber( const T &c) {
        return 1;
    }


    static  bool validate( const T &c) {
        return true;
    }

    static float dist(const T &lhs, const T &rhs) {
        T tDiff(lhs);
        tDiff - rhs;
        return dot(tDiff, tDiff, weights);
    }
};

    //weights for calculating distance between clusterItem components.
    //x position weight = 1
    //y position weignt = 1
    //hue value weight = 9
    //unused component weight = 0
    //sum of all weights = 11
    const float KMeansDataElementTraits<ClusterItem>::weights[] = {1,1,9,0,11};


//order the element according to the hue color
template<>
struct std::less<ClusterItem> : std::binary_function<ClusterItem, ClusterItem, bool> {
    bool operator()(const ClusterItem &l, const ClusterItem &r) {
        bool b = (l.data_[0]< r.data_[0] ) ? true: ( (l.data_[0] > r.data_[0] ) ? false: (
            (l.data_[1]< r.data_[1] ) ? true: ( (l.data_[1] > r.data_[1] ) ? false: (
                    (l.data_[2]< r.data_[2] ) ? true: ( (l.data_[2] > r.data_[2] ) ? false: (
                            (l.data_[3]< r.data_[3]) ? true : ( (l.data_[3] > r.data_[3] ) ? false: false
                                )
                            )
                        )
                    )
                )
            )
        );
        return b;

    }
};


void SpotIt::cluster(
                     const Mat &roiHueComponentImage,
                     const vector< vector< Point > > &contours,
                     const  vector<bool> &shouldProcessContour,
                     vector<int> &clusterAssignments,
                     int &numClusters,
                     float &sse) {
    
    //init
    map<int, int> indexReMapping;
    int numGoodContours=0;
    for(int i=0; i < shouldProcessContour.size(); ++i) {
        if(shouldProcessContour[i] && contours[i].size() > 0) {
            indexReMapping[numGoodContours++] = i;
        }
    }

    vector<ContourRepresentativePoint> contourRepPoints(numGoodContours);

    for(int j=0; j < contourRepPoints.size(); ++j) {
        map<int, int>::const_iterator mit = indexReMapping.find(j);
        assert(mit != indexReMapping.end());
        int indexInContours = mit->second;
        ContourRepresentativePoint &cRepPoint = contourRepPoints[j];
        cRepPoint.initializeFromContour(contours[indexInContours], roiHueComponentImage);
    }


    ContourRepresentativePoint::meanNormalization< vector<ContourRepresentativePoint>::iterator >(contourRepPoints.begin(), contourRepPoints.end());
    vector<ClusterItem> clusterItems(numGoodContours);
    for(int i=0; i < numGoodContours; ++i) {
        ClusterItem &ci = clusterItems[i];
        const ContourRepresentativePoint &cp = contourRepPoints[i];
        ci.data_[0] = cp.meanPoint.x;
        ci.data_[1] = cp.meanPoint.y;
        ci.data_[2] = cp.hsvColor[0];
    }


    //desired number of clusters
    int desiredK=9;
    KMeansClustering<ClusterItem> cluster(desiredK, clusterItems);
    cluster.doIt();
    
    sse = cluster.getOutputLastSSEForAllItems();
    const vector<int> &outputClusterAssignments = cluster.getOutputClusterAssignments();
    clusterAssignments.assign(contours.size(), -1);

    for(int j=0; j < outputClusterAssignments.size(); ++j) {
        map<int, int>::const_iterator mit = indexReMapping.find(j);
        assert(mit != indexReMapping.end());
        int indexInContours = mit->second;
        clusterAssignments[indexInContours] = outputClusterAssignments[j];
    }

    numClusters = desiredK;
    
    
    //verification that each valid contour is assigned to a cluster
    for(int j =0; j < outputClusterAssignments.size(); ++j) {
        assert(clusterAssignments[j] >= 0);
    }
    
    
}
