#include "common/PointCloud.h"
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>

namespace cstitcher {
namespace common {

PointCloud::PointCloud(){}

PointCloud::~PointCloud(){}


void PointCloud::open(std::string depthFile, std::string bgrFile, Camera& camera) {
    cv::Mat bgrImg = cv::imread(bgrFile, 1);
    CV_Assert(camera.bgrSize == bgrImg.size());
    cv::Mat depth = cv::imread(depthFile, CV_LOAD_IMAGE_ANYDEPTH);
    CV_Assert(depth.type() == CV_16UC1);
    CV_Assert(camera.depthSize == depth.size());

    cv::Mat depthImg;
    depth.convertTo(depthImg, CV_32FC1);
    depthImg *= 0.001;
    double depthFx, depthFy, depthCx, depthCy;
    depthFx = camera.depthCamera.at<double>(0, 0);
    depthFy = camera.depthCamera.at<double>(1, 1);
    depthCx = camera.depthCamera.at<double>(0, 2);
    depthCy = camera.depthCamera.at<double>(1, 2);

    double bgrFx, bgrFy, bgrCx, bgrCy;
    bgrFx = camera.bgrCamera.at<double>(0, 0);
    bgrFy = camera.bgrCamera.at<double>(1, 1);
    bgrCx = camera.bgrCamera.at<double>(0, 2);
    bgrCy = camera.bgrCamera.at<double>(1, 2);
    
    //create zbuffer
    cv::Mat zbuffer(depth.size(), CV_32FC2);
    zbuffer.setTo(cv::Vec2f(-1.0, -1.0));
    for(int v = 0; v < depthImg.rows; v++) {
        for(int u = 0; u < depthImg.cols; u++) {
            float z = depthImg.at<float>(v,u);
            cv::Vec3d p;
            p[2] = z;
            p[0] = (u - depthCx) *z* (1.0f / depthFx);
            p[1] = (v - depthCy) *z* (1.0f / depthFy);
            cv::Vec3d pCamera(p);
            pCamera = camera.extrinsicR * pCamera + camera.extrinsicT;
            if(z > 1e-7) {
                double uRGB = pCamera[0] * bgrFx/pCamera[2] + bgrCx;
                double vRGB = pCamera[1] * bgrFy /pCamera[2] + bgrCy;
                if (uRGB > 0.0 && uRGB < 640.0 && vRGB > 0.0 && vRGB < 480.0)
                {
                    cv::Vec2f vec = zbuffer.at<cv::Vec2f>(vRGB, uRGB);
                    if(vec[0] > 0) {
                        if(depthImg.at<float>(v,u) < depthImg.at<float>(vec[0], vec[1])) {
                            zbuffer.at<cv::Vec2f>(vec[0], vec[1]) = cv::Vec2f(v, u);
                        }
                    }
                    else
                    {
                        zbuffer.at<cv::Vec2f>(vRGB, uRGB) = cv::Vec2f(v, u);
                    }
                }
            }
        }
    }

    for(int v = 0; v < zbuffer.rows; v ++) {
        for(int u = 0; u < zbuffer.cols; u++) {
            cv::Vec2f vec = zbuffer.at<cv::Vec2f>(v, u);
            if(vec[0] > 0) {
                float z = depthImg.at<float>(v,u);
                cv::Vec3d p;
                p[2] = z;
                p[0] = (u - depthCx) *z* (1.0f / depthFx);
                p[1] = (v - depthCy) *z* (1.0f / depthFy);
                cv::Vec3d pCamera(p);
                pCamera = camera.extrinsicR * pCamera + camera.extrinsicT;
                if(z > 1e-7) {
                    double uRGB = pCamera[0] * bgrFx/pCamera[2] + bgrCx;
                    double vRGB = pCamera[1] * bgrFy /pCamera[2] + bgrCy;
                    if (uRGB > 0.0 && uRGB < 640.0 && vRGB > 0.0 && vRGB < 480.0)
                    {
                        cv::Point2i piRGB(uRGB, vRGB);
                        cv::Vec3b color = bgrImg.at<cv::Vec3b>(piRGB.y, piRGB.x);
                        points.push_back(p);
                        colors.push_back(cv::Vec3i(int(color[0]), int(color[1]), int(color[2])));
                    }
                }
            }
        }
    }
}

void PointCloud::save(const std::string& fileName) {
    std::ofstream os;
    os.open(fileName);
    os<<"ply\n";
    os<<"format ascii 1.0\n";
    os<<"comment : created from Kinect depth image\n";
    os<<"element vertex "<<points.size()<<"\n";
    os<<"property float x\n";
    os<<"property float y\n";
    os<<"property float z\n";
    os<<"property uchar blue\n";
    os<<"property uchar green\n";
    os<<"property uchar red\n";
    os<<"end_header\n";
    for(int i = 0; i < points.size(); i++) {
        os<<points[i][0]<<" "<<points[i][1]<<" "<<points[i][2]<<" "<<colors[i][0]<<" "<<colors[i][1]<<" "<<colors[i][2]<<"\n";
    }
    os.close();
}

void PointCloud::applyPose(const PoseRT& pose) {
    cv::Matx33d rvec = pose.getR();
    //    rvec = rvec.inv();
    cv::Vec3d tvec = pose.getT();
    //    tvec = tvec*(-1.0);
    for(int i = 0; i < points.size(); i++) {
        points[i] = rvec * points[i] + tvec;
    }
}

void PointCloud::getCloud(std::vector<cv::Vec3d>& points, std::vector<cv::Vec3i>& colors) {
    points = this->points;
    colors = this->colors;
}

int PointCloud::size() {
    return points.size();
}

}
}
