#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

	/***********************ofxCV*************************/
	vidGrabber.setVerbose(true);
	vidGrabber.setup(320, 240);
	vidGrabber.setDesiredFrameRate(0.1);

	colorImage.allocate(320, 240);
	grayImage.allocate(320, 240);
	grayBg.allocate(320, 240);
	grayDiff.allocate(320, 240);

	/***********************optical flow*************************/
	stepSize = 15;
	ySteps = vidGrabber.getHeight() / stepSize;
	xSteps = vidGrabber.getWidth() / stepSize;

	gui.setup();
	gui.add(lkMaxLevel.set("lkMaxLevel", 3, 0, 8));
	gui.add(lkMaxFeatures.set("lkMaxFeatures", 200, 1, 1000));
	gui.add(lkQualityLevel.set("lkQualityLevel", 0.1, 0.01, .2));
	gui.add(lkMinDistance.set("lkMinDistance", 4, 1, 16));
	gui.add(lkWinSize.set("lkWinSize", 8, 4, 64));
	gui.add(usefb.set("Use Farneback", true));
	gui.add(fbPyrScale.set("fbPyrScale", .5, 0, .99));
	gui.add(fbLevels.set("fbLevels", 4, 1, 8));
	gui.add(fbIterations.set("fbIterations", 2, 1, 8));
	gui.add(fbPolyN.set("fbPolyN", 7, 5, 10));
	gui.add(fbPolySigma.set("fbPolySigma", 1.5, 1.1, 2));
	gui.add(fbUseGaussian.set("fbUseGaussian", false));
	gui.add(fbWinSize.set("winSize", 32, 4, 64));

	curFlow = &fb;

	/**************************box2d*****************************/
	//Create a gravity environment
	box2d.init();
	box2d.setGravity(0, -20);
	box2d.createGround();
	box2d.createBounds(ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));

	circle.setPhysics(3.0, 0.5, 1.0);    //(float density, float bounce, float friction)
	//Be careful about the order of these two lines of code!
	circle.setup(box2d.getWorld(), 300, 300, 11);   //(b2World*b2world, float x, float y, float radius)

	ofBackground(250, 230, 209);
}

//--------------------------------------------------------------
void ofApp::update(){
	int t = 0;

	/******************optical flow******************/
	if (status == 0) {
		vidGrabber.update();
		colorImg = vidGrabber.getPixels();
		colorImg.mirror(false, true);
		if (vidGrabber.isFrameNew() && t % 13 == 0) {
			if (usefb) {
				curFlow = &fb;
				fb.setPyramidScale(fbPyrScale);
				fb.setNumLevels(fbLevels);
				fb.setWindowSize(fbWinSize);
				fb.setNumIterations(fbIterations);
				fb.setPolyN(fbPolyN);
				fb.setPolySigma(fbPolySigma);
				fb.setUseGaussian(fbUseGaussian);
			}
			else {
				curFlow = &lk;
				lk.setMaxFeatures(lkMaxFeatures);
				lk.setQualityLevel(lkQualityLevel);
				lk.setMinDistance(lkMinDistance);
				lk.setWindowSize(lkWinSize);
				lk.setMaxLevel(lkMaxLevel);
			}
			// you can use Flow polymorphically
			curFlow->calcOpticalFlow(colorImg);

			for (int y = 1; y + 1 < ySteps; y++) {
				for (int x = 1; x + 1 < xSteps; x++) {
					ofVec3f position(x * stepSize, y * stepSize, 0.0);
					ofRectangle area(position - ofVec3f(stepSize, stepSize, 0.0) / 2, stepSize, stepSize);
					offset = fb.getAverageFlowInRegion(area);
				}
			}
		}

		//emitting balls according to motions
		if (offset.y < -0.0002 || offset.y > 0.0002) {
			if (10 * offset.x < 0.039 && offset.x >0) {
				if (offset.x > 0) {
					ofSetColor(235, 123, 128);
				}
				else
				{
					ofSetColor(95, 185, 190);
				}

				auto circle = make_shared < ofxBox2dCircle>();
				circle->setPhysics(3.0, 0.5, 1.0);
				circle->setup(box2d.getWorld(), mouseX, mouseY, 10000 * offset.x);
				circle->addImpulseForce(ofVec2f(mouseX, mouseY), ofVec2f(10000 * offset.x, 10000 * offset.y));
				circles.push_back(circle);
			}
			else
			{
				if (offset.x > 0) {
					ofSetColor(235, 123, 128);
				}
				else
				{
					ofSetColor(95, 185, 190);
				}

				auto circle = make_shared < ofxBox2dCircle>();
				circle->setPhysics(3.0, 0.5, 1.0);
				circle->setup(box2d.getWorld(), mouseX, mouseY, 11);
				circle->addImpulseForce(ofVec2f(mouseX, mouseY), ofVec2f(10000 * offset.x, 10000 * offset.y));
				circles.push_back(circle);
			}

		}
	}
	/***********************ofxCV************************/
	else if (status >0) {
		vidGrabber.update();
		colorImage = vidGrabber.getPixels();
		colorImage.mirror(false, true);

		if (vidGrabber.isFrameNew() && t % 13 == 0) {
			colorImage.setFromPixels(vidGrabber.getPixels());
			colorImage.mirror(false, true);
			grayImage = colorImage; // convert our color image to a grayscale image
			grayDiff.absDiff(grayBg, grayImage);
			grayDiff.threshold(160);
			contourFinder.findContours(grayDiff);

			if (status == 2) {
				lines.clear();
				edges.clear();

				if (contourFinder.size() > 0) {
					for (int n = 0; n < contourFinder.size(); n++) {
						ofPolyline polyline = contourFinder.getPolyline(n);
						for (int i = 0; i < polyline.size(); i++) {
							polyline[i].x *= 3;
							polyline[i].y *= 3;
						}

						if (polyline.size() > 90)
						{
							lines.push_back(polyline);
						}

						for (int i = 0; i < lines.size(); i++) {
							ofPolyline poly = lines[i];
							poly.simplify();
							//cout << "poly.size=" << poly.size() << "\n";
							auto edge = make_shared<ofxBox2dEdge>();

							for (int j = 1; j < poly.size(); j = j + 9) {
								edge->addVertex(poly[j]);
							}

							edge->create(box2d.getWorld());
							edges.push_back(edge);
						}
					}
				}
			}
		}
	}

	t++;
	if (t >= 99) {
		t = 0;
	}
	/*********************box2d**********************/
	box2d.update();
}

//--------------------------------------------------------------
void ofApp::draw(){

	/**********************optical flow**************************/
	if (status == 0) {
		ofPushMatrix();
		//ofTranslate(250, 100);
		curFlow->draw(0, 0, 960, 720);
		ofPopMatrix();
	}

	/**************************ofxCV****************************/
	else if (status >0) {
		ofSetColor(235, 123, 128);
		ofPushMatrix();
		ofScale(3, 3); // scale the image by a factor of 2
		if (contourFinder.size() > 0) {
			for (int n = 0; n < contourFinder.size(); n++)
			{
				ofPolyline polyline = contourFinder.getPolyline(n);
				polyline.draw();
			}
		}
		ofPopMatrix();
	}
	
	/*************************box2d************************/
	for (auto circle : circles) {
		//instead of "for initialization; condition; increment"
		ofSetColor(235, 123, 128);
		ofFill();
		circle->draw();
	}
	for (auto & edge : edges) {
		ofSetColor(95, 185, 190);
		edge->draw();
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	if (button == 0) {
		status = 0; //emitting balls according to motions
		lines.clear();
		edges.clear();
		box2d.setGravity(0, -20);
	}
	else if(button == 2){
		status = 1; //emitting balls when right-clicked
		lines.clear();
		edges.clear();
		box2d.setGravity(0, -20);
		auto circle = make_shared < ofxBox2dCircle>();
		circle->setPhysics(3.0, 0.5, 1.0);
		circle->setup(box2d.getWorld(), mouseX, mouseY, 11);
		circles.push_back(circle);
	}
	else
	{
		status = 2; //falling balls
		box2d.setGravity(0, 20);
	}
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
