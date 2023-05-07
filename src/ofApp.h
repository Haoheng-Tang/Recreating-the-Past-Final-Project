#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxOpenCv.h"
#include "ofxGui.h"
#include "ofxBox2d.h"

using namespace ofxCv;
using namespace cv;

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
		

		/***********************optical flow************************/
		//ofVideoGrabber camera;
		ofImage colorImg;
		
		ofxCv::FlowFarneback fb;
		ofxCv::FlowPyrLK lk;

		ofxCv::Flow* curFlow;
		ofVec2f offset;

		int stepSize, xSteps, ySteps;

		ofxPanel gui;
		ofParameter<float> fbPyrScale, lkQualityLevel, fbPolySigma;
		ofParameter<int> fbLevels, lkWinSize, fbIterations, fbPolyN, fbWinSize, lkMaxLevel, lkMaxFeatures, lkMinDistance;
		ofParameter<bool> fbUseGaussian, usefb;


		/*************************ofxCV**************************/
		ofVideoGrabber vidGrabber;
		ofxCvColorImage colorImage;
		ofxCvGrayscaleImage grayImage, grayBg, grayDiff;
		ContourFinder contourFinder;


		/**************************box2d*****************************/
		ofxBox2d box2d;
		ofxBox2dCircle circle;
		vector <shared_ptr<ofxBox2dCircle>> circles;
		vector <ofPolyline>                  lines;
		vector <shared_ptr<ofxBox2dEdge>>    edges;

		int status = 1;
};
