#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	/*******************************optical flow****************************************/
	ofSetVerticalSync(true);
	ofSetLogLevel(OF_LOG_NOTICE);
	
	densityWidth = 1280;
	densityHeight = 720;
	simulationWidth = densityWidth;
	simulationHeight = densityHeight;
	windowWidth = ofGetWindowWidth();
	windowHeight = ofGetWindowHeight();
	// process the average on 1/16 resolution
	avgWidth = densityWidth / 4;
	avgHeight = densityHeight / 4;
	
	opticalFlow.setup(simulationWidth, simulationHeight);
	combinedBridgeFlow.setup(simulationWidth, simulationHeight, densityWidth, densityHeight);
	pixelFlow.setup(avgWidth, avgHeight, FT_VELOCITY);
	averageFlows.resize(numRegios);
	for (int i = 0; i < numRegios; i++) {
		averageFlows[i].setup(avgWidth, avgHeight, FT_VELOCITY);
		averageFlows[i].setRoi(1.0 / (numRegios + 1.0) * (i + 1), .2, 1.0 / (numRegios + 2.0), .6);
	}
	
	flows.push_back(&opticalFlow);
	flows.push_back(&combinedBridgeFlow);
	for (auto& f : averageFlows) { flows.push_back(&f); }

	for (auto flow : flows) { flow->setVisualizationFieldSize(glm::vec2(simulationWidth / 2, simulationHeight / 2)); }

	mouseFlows.push_back(&densityMouseFlow);
	mouseFlows.push_back(&velocityMouseFlow);
	
	simpleCam.setup(densityWidth, densityHeight, true);
	cameraFbo.allocate(densityWidth, densityHeight);
	ftUtil::zero(cameraFbo);

	lastTime = ofGetElapsedTimef();
	
	setupGui();



	/*******************************Box2d****************************************/
	//Create a gravity environment
	box2d.init();
	box2d.setGravity(0, -45);
	box2d.createGround();
	box2d.createBounds(ofRectangle(0, 0, ofGetWidth(), ofGetHeight()));

	circle.setPhysics(3.0, 0.5, 1.0);    //(float density, float bounce, float friction)
	//Be careful about the order of these two lines of code!
	circle.setup(box2d.getWorld(), 300, 300, 11);   //(b2World*b2world, float x, float y, float radius)

	ofBackground(250, 230, 209);
}

//--------------------------------------------------------------
void ofApp::setupGui() {
	
	gui.setup("settings");
	gui.setDefaultBackgroundColor(ofColor(255, 255, 255, 127));
	gui.setDefaultFillColor(ofColor(100, 180, 255, 127));
	gui.add(toggleGuiDraw.set("show gui (G)", true));
	gui.add(toggleCameraDraw.set("draw camera (C)", true));
	gui.add(outputWidth.set("output width", 1280, 256, 1920));
	gui.add(outputHeight.set("output height", 720, 144, 1080));
	gui.add(simulationScale.set("simulation scale", 2, 1, 4));
	gui.add(simulationFPS.set("simulation fps", 60, 1, 60));
	outputWidth.addListener(this, &ofApp::simulationResolutionListener);
	outputHeight.addListener(this, &ofApp::simulationResolutionListener);
	simulationScale.addListener(this, &ofApp::simulationResolutionListener);
	
	visualizationParameters.setName("visualization");
	visualizationParameters.add(visualizationMode.set("mode", FLUID_DEN, INPUT_FOR_DEN, FLUID_DEN));
	visualizationParameters.add(visualizationScale.set("scale", 1, 0.1, 10.0));
	visualizationParameters.add(toggleVisualizationScalar.set("show scalar", false));
	visualizationMode.addListener(this, &ofApp::visualizationModeListener);
	toggleVisualizationScalar.addListener(this, &ofApp::toggleVisualizationScalarListener);
	visualizationScale.addListener(this, &ofApp::visualizationScaleListener);
	
	bool s = true;
	gui.add(visualizationParameters);
	for (auto flow : flows) {
		gui.add(flow->getParameters());
	}

	if (!ofFile("settings.xml")) { gui.saveToFile("settings.xml"); }	
	gui.loadFromFile("settings.xml");
	
	gui.minimizeAll();
	toggleGuiDraw = true;
}

//--------------------------------------------------------------
void ofApp::update(){
	/*******************************optical flow****************************************/
	ofSetFrameRate(simulationFPS);
	float dt = 1.0 / max(ofGetFrameRate(), 1.f); // more smooth as 'real' deltaTime.
	
	simpleCam.update();
	if (simpleCam.isFrameNew()) {
		cameraFbo.begin();
		simpleCam.draw(cameraFbo.getWidth(), 0, -cameraFbo.getWidth(), cameraFbo.getHeight());  // draw flipped
		cameraFbo.end();
		
		opticalFlow.setInput(cameraFbo.getTexture());
	}
	
	opticalFlow.update();
	
	combinedBridgeFlow.setVelocity(opticalFlow.getVelocity());
	combinedBridgeFlow.setDensity(cameraFbo.getTexture());
	combinedBridgeFlow.update(dt);

	pixelFlow.setInput(opticalFlow.getVelocity());
	for (auto flow : mouseFlows) { if (flow->didChange() && flow->getType() == FT_VELOCITY) { pixelFlow.addInput(flow->getTexture()); } }
	pixelFlow.update();
	for (auto& f : averageFlows) { f.update(pixelFlow.getPixels()); }


	/*******************************Box2d****************************************/
	box2d.update();
		ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	float minDis = ofGetMousePressed() ? 300 : 200;

	for (auto &circle : circles) {
		float dis = mouse.distance(circle->getPosition());

		if (dis < minDis) {
			//circle->addRepulsionForce(mouse, 1.2);
			circle->addImpulseForce(mouse, ofVec2f(3,3));
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	/*******************************optical flow****************************************/
	ofClear(0,0);
	
	ofPushStyle();
	if (toggleCameraDraw.get()) {
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		cameraFbo.draw(0, 0, windowWidth, windowHeight);
	}
	
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	switch(visualizationMode.get()) {
		case INPUT_FOR_DEN:	combinedBridgeFlow.drawInput(0, 0, windowWidth, windowHeight); break;
		case INPUT_FOR_VEL:	opticalFlow.drawInput(0, 0, windowWidth, windowHeight); break;
		case FLOW_VEL:		opticalFlow.draw(0, 0, windowWidth, windowHeight); break;
		case BRIDGE_VEL:	combinedBridgeFlow.drawVelocity(0, 0, windowWidth, windowHeight); break;
		case BRIDGE_DEN:	combinedBridgeFlow.drawDensity(0, 0, windowWidth, windowHeight); break;
		case BRIDGE_TMP:	combinedBridgeFlow.drawTemperature(0, 0, windowWidth, windowHeight); break;
		case BRIDGE_PRS:	break;
		default: break;
	}
	
	ofEnableBlendMode(OF_BLENDMODE_SUBTRACT);
	
	if (toggleGuiDraw) {
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		drawGui();
	}
	ofPopStyle();


	/*******************************Box2d****************************************/
	for (auto circle : circles) {
		//instead of "for initialization; condition; increment"
		ofFill();
		circle->draw();
	}
}

//--------------------------------------------------------------
void ofApp::drawGui() {
	guiFPS = (int)(ofGetFrameRate() + 0.5);
	
	// calculate minimum fps
	float deltaTime = ofGetElapsedTimef() - lastTime;
	lastTime = ofGetElapsedTimef();
	deltaTimeDeque.push_back(deltaTime);
	
	while (deltaTimeDeque.size() > guiFPS.get())
		deltaTimeDeque.pop_front();
	
	float longestTime = 0;
	for (int i=0; i<(int)deltaTimeDeque.size(); i++){
		if (deltaTimeDeque[i] > longestTime)
			longestTime = deltaTimeDeque[i];
	}
	
	guiMinFPS.set(1.0 / longestTime);
	
	ofPushStyle();
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
	gui.draw();
	ofPopStyle();
}


//--------------------------------------------------------------
void ofApp::simulationResolutionListener(int &_value){
	densityWidth = outputWidth;
	densityHeight = outputHeight;
	simulationWidth = densityWidth / simulationScale;
	simulationHeight = densityHeight / simulationScale;
	
	opticalFlow.resize(simulationWidth, simulationHeight);
	combinedBridgeFlow.resize(simulationWidth, simulationHeight, densityWidth, densityHeight);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

	/*******************************Box2d****************************************/
	if (button == 0) {
		ofSetColor(95, 185, 190);
	}
	else
	{
		ofSetColor(235, 123, 128);
	}
	auto circle = make_shared < ofxBox2dCircle>();
	circle->setPhysics(3.0, 0.5, 1.0);
	circle->setup(box2d.getWorld(), mouseX, mouseY, 11);
	circles.push_back(circle);
}

