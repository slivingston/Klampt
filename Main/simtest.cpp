#include "SimViewProgram.h"
#include "View/ViewPlot.h"
#include <utils/stringutils.h>
#include <utils/AnyCollection.h>
#include <robotics/IKFunctions.h>
//#include <GL/glut.h>
#include <glui.h>
#include <fstream>
using namespace Math3D;
using namespace GLDraw;

enum {
  SIMULATE_BUTTON_ID,
  SAVE_MOVIE_BUTTON_ID,
  RESET_BUTTON_ID,
  IO_DATA_CHOICE_ID,
  SAVE_ID,
  LOAD_ID,
  COMMAND_MILESTONE_BUTTON_ID,
  SETTINGS_LISTBOX_ID,
  SETTINGS_ID,
  COMMAND_ID,
  SENSOR_LISTBOX_ID,
  SENSOR_MEASUREMENT_LISTBOX_ID,
  DRAW_SENSOR_CHECKBOX_ID,
  ISOLATE_MEASUREMENT_ID,
  DRAW_EXPANDED_CHECKBOX_ID,
  DRIVER_LISTBOX_ID,
  DRIVER_SPINNER_ID0,
  DRIVER_SPINNER_ID1,
  DRIVER_SPINNER_ID2,
  DRIVER_SPINNER_ID3,
  DRIVER_SPINNER_ID4,
  DRIVER_SPINNER_ID5,
  DRIVER_SPINNER_ID6,
  DRIVER_SPINNER_ID7,
};

enum {
  IO_SIM_STATE_ID,
  IO_LINEAR_PATH_ID,
  IO_MILESTONES_ID,
  IO_MULTIPATH_ID,
  IO_RAW_COMMANDS_ID,
};


void delete_all(GLUI_Listbox* listbox)
{
  while(listbox->items_list.first_child()) {
    listbox->delete_item(((GLUI_Listbox_Item*)listbox->items_list.first_child())->id);
  }
}

class ProgramSettings : public AnyCollection
{
public:
  ProgramSettings() {  }
  bool read(const char* fn) {
    ifstream in(fn,ios::in);
    if(!in) return false;
    AnyCollection newEntries;
    if(!newEntries.read(in)) return false;
    merge(newEntries);
    return true;
  }
  bool write(const char* fn) {
    ofstream out(fn,ios::out);
    if(!out) return false;
    AnyCollection::write(out);
    out.close();
    return true;
  }
};


class SimTestProgram : public SimViewProgram
{
public:
  ProgramSettings settings;

  //GUI state
  GLUI* glui;
  vector<GLUI_Spinner*> driver_spinners;
  vector<float> spinner_values;
  int driverValueIndex;
  int pose_ik;
  vector<IKGoal> poseGoals;
  int currentPoseGoal;
  int io_choice;
  GLUI_String file_name;
  int log_check;

  Config poseConfig;
  RobotInfo* hoverRobot;
  RigidObjectInfo* hoverObject;
  int hoverLink;
  Vector3 hoverPt;
  bool forceApplicationMode,forceSpringActive;
  Vector3 forceSpringAnchor;
  Config tempq;
  int drawBBs,drawPoser,drawDesired,drawEstimated,drawContacts,drawWrenches,drawSensorPlot,drawExpanded;
  vector<string> controllerSettings;
  int controllerSettingIndex;
  GLUI_EditText* settingEdit;
  vector<string> controllerCommands;
  int controllerCommandIndex;
  GLUI_EditText* commandEdit;

  ViewPlot sensorPlot;
  int sensorSelectIndex;
  int sensorMeasurementSelectIndex;
  vector<bool> drawMeasurement;
  GLUI_Listbox* sensorMeasurementBox;
  GLUI_Checkbox* drawMeasurementCheckbox;

  vector<GeometryAppearance> originalAppearance,expandedAppearance;

  SimTestProgram(RobotWorld* world)
    :SimViewProgram(world)
  {
    settings["movieWidth"] = 640;
    settings["movieHeight"] = 480;
    settings["updateStep"] = 0.01;
    settings["pathDelay"] = 0.5;
    settings["sensorPlot"]["x"]=20;
    settings["sensorPlot"]["y"]=20;
    settings["sensorPlot"]["width"]=300;
    settings["sensorPlot"]["height"]=200;
    settings["sensorPlot"]["autotime"]=false;
    settings["sensorPlot"]["duration"]=2.0;
    settings["poser"]["color"][0] = 1;
    settings["poser"]["color"][1] = 1;
    settings["poser"]["color"][2] = 0;
    settings["poser"]["color"][3] = 0.5;
    settings["desired"]["color"][0] = 0;
    settings["desired"]["color"][1] = 1;
    settings["desired"]["color"][2] = 0;
    settings["desired"]["color"][3] = 0.5;
    settings["contact"]["pointSize"] = 5;
    settings["contact"]["normalLength"] = 0.05;
    settings["contact"]["forceScale"] = 0.01;
  }

  virtual bool Initialize()
  {
    if(!settings.read("simtest.settings")) {
      printf("Didn't read settings from simtest.settings\n");
      printf("Writing default settings to simtest_default.settings\n");
      settings.write("simtest_default.settings");
    }

    drawBBs = 0;
    drawPoser = 1;
    drawDesired = 0;
    drawEstimated = 0;
    drawContacts = 0;
    drawWrenches = 1;
    drawSensorPlot = 0;
    drawExpanded = 0;
    log_check = 0;
    forceApplicationMode = false, forceSpringActive = false;
    map<string,string> csettings = sim.robotControllers[0]->Settings();
    controllerCommands = sim.robotControllers[0]->Commands();

    /*
    //TEMP: testing determinism
    bool nondet = false;
    int n=20;
    Real dt = 0.01;
    vector<string> trace(n);
    for(int i=0;i<n;i++) {
      string start,temp;
      sim.WriteState(start);
      sim.Advance(dt);
      sim.WriteState(trace[i]);

      sim.ReadState(start);
      sim.Advance(dt);
      sim.WriteState(temp);
      if(temp != trace[i]) {
	printf("Warning, sim state nondeterministic @ step %d: ",i);
	nondet = true;
	if(temp.length() != trace[i].length())
	  printf("different lengths %d vs %d\n",temp.length(),trace[i].length());
	else {
	  for(size_t j=0;j<temp.length();j++)
	    if(temp[j] != trace[i][j]) {
	      printf("byte %d\n",j);
	      break;
	    }
	}
	getchar();
      }
    }
    */

    hoverRobot=NULL;
    hoverObject=NULL;
    hoverLink=-1;
    hoverPt.setZero();

    if(!WorldViewProgram::Initialize()) return false;

    Robot* robot=world->robots[0].robot;
    poseConfig = robot->q;
    pose_ik = 0;
    currentPoseGoal = -1;
    io_choice = IO_SIM_STATE_ID;
    file_name="robot_commands.log";
    //Old GLUI version
    //strcpy(file_name,"robot_commands.log");

    glui = GLUI_Master.create_glui_subwindow(main_window,GLUI_SUBWINDOW_RIGHT);
    glui->set_main_gfx_window(main_window);
    glui->add_button("Simulate",SIMULATE_BUTTON_ID,ControlFunc);
    glui->add_button("Reset",RESET_BUTTON_ID,ControlFunc);
    glui->add_button("Save movie",SAVE_MOVIE_BUTTON_ID,ControlFunc);
    glui->add_checkbox("Log simulation state",&log_check);
    GLUI_Panel* panel = glui->add_rollout("Input/output");
    GLUI_Listbox* group=glui->add_listbox_to_panel(panel,"Item type",&io_choice,IO_DATA_CHOICE_ID,ControlFunc);
    group->add_item(0,"Simulation state");
    group->add_item(1,"Piecewise linear path");
    group->add_item(2,"Milestone path");
    group->add_item(3,"Multipath");
    group->add_item(4,"Raw command log");
    glui->add_button_to_panel(panel,"Save",SAVE_ID,ControlFunc);
    glui->add_button_to_panel(panel,"Load",LOAD_ID,ControlFunc);
    glui->add_edittext_to_panel(panel,"File",GLUI_EDITTEXT_STRING,&file_name);
    glui->add_checkbox("Draw poser",&drawPoser);
    glui->add_checkbox("Draw desired",&drawDesired);
    glui->add_checkbox("Draw estimated",&drawEstimated);
    glui->add_checkbox("Draw bboxes",&drawBBs);
    glui->add_checkbox("Draw contacts",&drawContacts);
    glui->add_checkbox("Draw expanded",&drawExpanded,DRAW_EXPANDED_CHECKBOX_ID,ControlFunc);
    glui->add_checkbox("Draw wrenches",&drawWrenches);
    panel = glui->add_rollout("Controller settings");
    if(!csettings.empty()) {
      for(map<string,string>::iterator i=csettings.begin();i!=csettings.end();i++) {
	controllerSettings.push_back(i->first);
      }
      controllerSettingIndex = 0;
      GLUI_Listbox* settingsBox = glui->add_listbox_to_panel(panel,"Setting",&controllerSettingIndex,SETTINGS_LISTBOX_ID,ControlFunc);
      for(size_t i=0;i<controllerSettings.size();i++) {
	settingsBox->add_item((int)i,controllerSettings[i].c_str());
      }
      settingEdit=glui->add_edittext_to_panel(panel,"Value",GLUI_EDITTEXT_TEXT,NULL,SETTINGS_ID,ControlFunc);
      settingEdit->set_text(csettings[controllerSettings[0]].c_str());
    }
    controllerCommandIndex = 0;
    GLUI_Listbox* commandsBox = glui->add_listbox_to_panel(panel,"Command",&controllerCommandIndex);
    for(size_t i=0;i<controllerCommands.size();i++) {
      commandsBox->add_item((int)i,controllerCommands[i].c_str());
    }
    commandEdit = glui->add_edittext_to_panel(panel,"Arg",GLUI_EDITTEXT_TEXT,NULL,COMMAND_ID,ControlFunc);
    commandEdit->set_text("");

    //sensors panel
    panel = glui->add_rollout("Sensors");
    sensorSelectIndex = 0;
    sensorMeasurementSelectIndex = 0;
    GLUI_Listbox* sensorBox = glui->add_listbox_to_panel(panel,"Sensor",&sensorSelectIndex,SENSOR_LISTBOX_ID,ControlFunc);
    RobotSensors& sensors = sim.controlSimulators[0].sensors;
    for(size_t i=0;i<sensors.sensors.size();i++)
      sensorBox->add_item(i,sensors.sensors[i]->name.c_str());
    drawMeasurementCheckbox = glui->add_checkbox_to_panel(panel,"Show",&drawSensorPlot);
    sensorMeasurementBox = glui->add_listbox_to_panel(panel,"Measurement",&sensorMeasurementSelectIndex,SENSOR_MEASUREMENT_LISTBOX_ID,ControlFunc);
    drawMeasurementCheckbox = glui->add_checkbox_to_panel(panel,"Plot value",NULL,DRAW_SENSOR_CHECKBOX_ID,ControlFunc);
    glui->add_button_to_panel(panel,"Isolate",ISOLATE_MEASUREMENT_ID,ControlFunc);
    SyncSensorMeasurements();
    sensorPlot.x = int(settings["sensorPlot"]["x"]);
    sensorPlot.y = int(settings["sensorPlot"]["y"]);
    sensorPlot.width = int(settings["sensorPlot"]["width"]);
    sensorPlot.height = int(settings["sensorPlot"]["height"]);
    sensorPlot.autoXRange = bool(settings["sensorPlot"]["autotime"]);
    sensorPlot.xmin = sim.time-Real(settings["sensorPlot"]["duration"]);
    sensorPlot.xmax = sim.time;

    //posing panel
    panel = glui->add_rollout("Robot posing");

    glui->add_checkbox_to_panel(panel,"Pose by IK",&pose_ik);
    if(robot->drivers.size() > 8) {
      driverValueIndex = 0;
      driver_spinners.resize(1);
      spinner_values.resize(1);
      GLUI_Listbox* driverBox = glui->add_listbox_to_panel(panel,"Driver",&driverValueIndex,DRIVER_LISTBOX_ID,ControlFunc);
      for(size_t i=0;i<robot->drivers.size();i++) {
	driverBox->add_item((int)i,robot->driverNames[i].c_str());
      }
      driver_spinners[0] = glui->add_spinner_to_panel(panel,"Value",GLUI_SPINNER_FLOAT,&spinner_values[0],DRIVER_SPINNER_ID0);
      Vector2 limits=robot->GetDriverLimits(driverValueIndex);
      driver_spinners[0]->set_float_limits(limits.x,limits.y);
      SyncDriverSpinners();
    }
    else {
      driverValueIndex = -1;
      driver_spinners.resize(robot->drivers.size());
      spinner_values.resize(driver_spinners.size());
      for(size_t i=0;i<driver_spinners.size();i++) {
	char buf[1024];
	strcpy(buf,robot->driverNames[i].c_str());
	driver_spinners[i] = glui->add_spinner_to_panel(panel,buf,GLUI_SPINNER_FLOAT,&spinner_values[i],DRIVER_SPINNER_ID0+(int)i,ControlFunc);
	Vector2 limits=robot->GetDriverLimits(i);
	driver_spinners[i]->set_float_limits(limits.x,limits.y);
      }
    }

    glui->add_button_to_panel(panel,"Set milestone",COMMAND_MILESTONE_BUTTON_ID,ControlFunc);
    printf("Starting glui...\n");
    return true;
  }

  void SyncSensorMeasurements()
  {
    RobotSensors& sensors = sim.controlSimulators[0].sensors;
    SensorBase* sensor = NULL;
    sensor = sensors.sensors[sensorSelectIndex];
    delete_all(sensorMeasurementBox);
    vector<string> names;
    sensor->MeasurementNames(names);
    for(size_t i=0;i<names.size();i++) {
      sensorMeasurementBox->add_item(i,names[i].c_str());
    }
    drawMeasurement.resize(names.size());
    fill(drawMeasurement.begin(),drawMeasurement.end(),true);
    sensorPlot.curves.resize(0);
    const static float colors[12][3] = {{1,0,0},{1,0.5,0},{1,1,0},{0.5,1,0},{0,1,0},{0,1,0.5},{0,1,1},{0,0.5,1},{0,0,1},{0.5,0,1},{1,0,1},{1,0,0.5}};
    sensorPlot.curveColors.resize(Min((size_t)12,names.size()));
    for(size_t i=0;i<sensorPlot.curveColors.size();i++)
      sensorPlot.curveColors[i].set(colors[i][0],colors[i][1],colors[i][2]);
    size_t start=sensorPlot.curveColors.size();
    sensorPlot.curveColors.resize(names.size());
    for(size_t i=start;i<sensorPlot.curveColors.size();i++) 
      sensorPlot.curveColors[i].setHSV(Rand()*360.0,1,1);

    SyncSensorMeasurementDraw();
  }

  void SyncSensorMeasurementDraw()
  {
    if(sensorMeasurementSelectIndex >= (int)drawMeasurement.size()) return;
    if(drawMeasurement[sensorMeasurementSelectIndex]) {
      drawMeasurementCheckbox->set_int_val(1);
      sensorPlot.curveColors[sensorMeasurementSelectIndex].rgba[3] = 1;
    }
    else {
      drawMeasurementCheckbox->set_int_val(0);
      sensorPlot.curveColors[sensorMeasurementSelectIndex].rgba[3] = 0;
    }
    sensorPlot.AutoYRange();
    Refresh();
  }

  void SyncSensorMeasurementDrawToCheckbox()
  {
    if(sensorMeasurementSelectIndex < (int)drawMeasurement.size()) {
      drawMeasurement[sensorMeasurementSelectIndex] = (drawMeasurementCheckbox->get_int_val()!=0);
      sensorPlot.curveColors[sensorMeasurementSelectIndex].rgba[3] = drawMeasurementCheckbox->get_int_val();
      sensorPlot.AutoYRange();
      Refresh();
    }
  }

  void IsolateSensorMeasurementDraw()
  {
    if(sensorMeasurementSelectIndex < (int)drawMeasurement.size()) {
      for(size_t i=0;i<drawMeasurement.size();i++) {
	drawMeasurement[i] = false;
	sensorPlot.curveColors[i].rgba[3] = 0;
      }
      drawMeasurement[sensorMeasurementSelectIndex] = true;
      sensorPlot.curveColors[sensorMeasurementSelectIndex].rgba[3] = 1.0;
      sensorPlot.AutoYRange();
      Refresh();
    }
  }

  void SyncDriverSpinners()
  {
    Robot* robot=world->robots[0].robot;
    if(driverValueIndex >= 0) {
      spinner_values[0] = robot->GetDriverValue(driverValueIndex);
    }
    else {
      for(size_t i=0;i<driver_spinners.size();i++) {
	spinner_values[i] = robot->GetDriverValue(i);
      }
      GLUI_Master.sync_live_all();
    }
  }

  virtual void RenderWorld()
  {
    SimViewProgram::RenderWorld();

    glEnable(GL_BLEND);
    Robot* robot=world->robots[0].robot;
    RobotController* rc=sim.robotControllers[0];
    if(drawPoser) {
      //draw desired milestone
      robot->UpdateConfig(poseConfig);
      AnyCollection poserColorSetting = settings["poser"]["color"];
      GLColor poserColor(poserColorSetting[0],poserColorSetting[1],poserColorSetting[2],poserColorSetting[3]);
      world->robots[0].view.SetColors(poserColor);
      for(size_t j=0;j<robot->links.size();j++) {
	world->robots[0].view.DrawLink_World(j);
      }
    }
    //applying a force
    if(forceSpringActive && hoverRobot && hoverLink >= 0) {
      Robot* robot=hoverRobot->robot;
      RigidTransform T;
      sim.odesim.robot(0)->GetLinkTransform(hoverLink,T);
      Vector3 wp = T*hoverPt;
      glDisable(GL_LIGHTING);
      glColor3f(1,0.5,0);
      glPointSize(5.0);
      glBegin(GL_POINTS);
      glVertex3v(wp);
      glVertex3v(forceSpringAnchor);
      glEnd();
      glLineWidth(3.0);
      glBegin(GL_LINES);
      glVertex3v(wp);
      glVertex3v(forceSpringAnchor);
      glEnd();
      glLineWidth(1.0);
      glEnable(GL_LIGHTING);
    }
    //hover robot is being positioned
    if(hoverRobot && hoverRobot != &world->robots[0] && tempq.n!=0) {
      Config oldq = hoverRobot->robot->q;
      if(tempq.n != hoverRobot->robot->q.n) 
	printf("Warning, tempq size %d, robot size %d\n",tempq.n,hoverRobot->robot->q.n);
      else
	hoverRobot->robot->UpdateConfig(tempq);
      hoverRobot->view.SetColors(GLColor(1,1,0,0.5));
      for(size_t j=0;j<hoverRobot->robot->links.size();j++) {
	hoverRobot->view.DrawLink_World(j);
      }
      hoverRobot->robot->UpdateConfig(oldq);
    }
    if(pose_ik) {
      Robot* robot = world->robots[0].robot;
      Config oldq = robot->q;
      robot->UpdateConfig(poseConfig);
      for(size_t i=0;i<poseGoals.size();i++) {
	Vector3 curpos = robot->links[poseGoals[i].link].T_World*poseGoals[i].localPosition;
	Vector3 despos = poseGoals[i].endPosition;
	glDisable(GL_LIGHTING);
	glColor3f(1,0,0);
	glLineWidth(5.0);
	glBegin(GL_LINES);
	glVertex3v(curpos);
	glVertex3v(despos);
	glEnd();
	glLineWidth(1.0);

	glEnable(GL_LIGHTING);
	float color[4] = {1,0,0,1};
	glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,color); 
	glPushMatrix();
	glTranslate(curpos);
	if(poseGoals[i].rotConstraint == IKGoal::RotFixed)
	  drawBox(0.05,0.05,0.05);
	else
	  drawSphere(0.025,16,8);
	glPopMatrix();
	float color2[4] = {1,0.5,0,1};
	glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE,color2); 
	glPushMatrix();
	glTranslate(despos);
	if(poseGoals[i].rotConstraint == IKGoal::RotFixed)
	  drawBox(0.04,0.04,0.04);
	else
	  drawSphere(0.02,16,8);
	glPopMatrix();
      }
      robot->UpdateConfig(oldq);
    }
    //draw commanded setpoint
    if(drawDesired) {
      robot->UpdateConfig(poseConfig);
      sim.controlSimulators[0].GetCommandedConfig(robot->q);
      robot->UpdateFrames();
      AnyCollection desiredColorSetting = settings["desired"]["color"];
      GLColor desiredColor(desiredColorSetting[0],desiredColorSetting[1],desiredColorSetting[2],desiredColorSetting[3]);
      world->robots[0].view.SetColors(desiredColor);
      for(size_t j=0;j<robot->links.size();j++) 
	world->robots[0].view.DrawLink_World(j);
    }
    if(drawEstimated) {
    }

    //draw collision feedback
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glEnable(GL_POINT_SMOOTH);
    if(drawContacts) {
      glDisable(GL_DEPTH_TEST);
      Real pointSize = settings["contact"]["pointSize"];
      Real fscale=settings["contact"]["forceScale"];
      Real nscale=settings["contact"]["normalLength"];
      DrawContacts(pointSize,fscale,nscale);
    }

    if(drawWrenches) {
      Real robotsize = 1.5;
      Real fscale = robotsize/(robot->GetTotalMass()*9.8);
      DrawWrenches(fscale);
    }

    //draw bounding boxes
    if(drawBBs) {
      sim.UpdateModel();
      for(size_t i=0;i<world->robots.size();i++) {
	for(size_t j=0;j<world->robots[i].robot->geometry.size();j++) {
	  if(world->robots[i].robot->geometry[j].Empty()) continue;
	  Box3D bbox = world->robots[i].robot->geometry[j].GetBB();
	  Matrix4 basis;
	  bbox.getBasis(basis);
	  glColor3f(1,0,0);
	  drawOrientedWireBox(bbox.dims.x,bbox.dims.y,bbox.dims.z,basis);
	}
      }
      for(size_t i=0;i<world->rigidObjects.size();i++) {
	Box3D bbox = world->rigidObjects[i].object->geometry.GetBB();
	Matrix4 basis;
	bbox.getBasis(basis);
	glColor3f(1,0,0);
	drawOrientedWireBox(bbox.dims.x,bbox.dims.y,bbox.dims.z,basis);
      }
      for(size_t i=0;i<world->terrains.size();i++) {
	Box3D bbox = world->terrains[i].terrain->geometry.GetBB();
	Matrix4 basis;
	bbox.getBasis(basis);
	glColor3f(1,0.5,0);
	drawOrientedWireBox(bbox.dims.x,bbox.dims.y,bbox.dims.z,basis);
      }
    }
  }

  void RenderScreen()
  {
    if(drawSensorPlot)
      sensorPlot.DrawGL();
  }

  void ToggleDrawExpandedCheckbox()
  {
    if(originalAppearance.empty()) {
      //first call -- initialize display lists
      originalAppearance.resize(world->NumIDs());
      expandedAppearance.resize(world->NumIDs());
      vector<Real> paddings(world->NumIDs(),0);
      for(size_t i=0;i<originalAppearance.size();i++) {
	//robots don't get a geometry
	if(world->IsRobot(i) >= 0) continue;
	originalAppearance[i] = world->GetAppearance(i);
	expandedAppearance[i].geom = originalAppearance[i].geom;
	expandedAppearance[i].faceColor = originalAppearance[i].faceColor;
	expandedAppearance[i].lightFaces = true;
	expandedAppearance[i].drawFaces = true;
	expandedAppearance[i].drawVertices = false;
	paddings[i] = world->GetGeometry(i).margin;
      }
      for(size_t i=0;i<world->robots.size();i++) {
	for(size_t j=0;j<world->robots[i].robot->links.size();j++) {
	  int id=world->RobotLinkID(i,j);
	  if(sim.odesim.robot(i)->triMesh(j)) 
	    paddings[id] += sim.odesim.robot(i)->triMesh(j)->GetPadding();
	}
      }
      for(size_t i=0;i<world->terrains.size();i++) {
	int id=world->TerrainID(i);
	if(sim.odesim.envGeom(i)) 
	  paddings[id] += sim.odesim.envGeom(i)->GetPadding();
      }
      for(size_t i=0;i<world->rigidObjects.size();i++) {
	int id = world->RigidObjectID(i);
	if(sim.odesim.object(i)->triMesh()) 
	  paddings[id] += sim.odesim.object(i)->triMesh()->GetPadding();
      }
      for(size_t i=0;i<originalAppearance.size();i++) {
	//robots don't get a geometry
	if(world->IsRobot(i) >= 0) continue;
	if(paddings[i] > 0) {
	  //draw the expanded mesh
	  expandedAppearance[i].faceDisplayList.beginCompile();
	  GLDraw::drawExpanded(world->GetGeometry(i),paddings[i]);
	  expandedAppearance[i].faceDisplayList.endCompile();
	}
	else
	  expandedAppearance[i] = originalAppearance[i];
      }
    }
    if(drawExpanded) {
      for(size_t i=0;i<originalAppearance.size();i++) {
	//robots don't get a geometry
	if(world->IsRobot(i) >= 0) continue;
	world->GetAppearance(i) = expandedAppearance[i];
      }
    }
    else {
      for(size_t i=0;i<originalAppearance.size();i++) {
	//robots don't get a geometry
	if(world->IsRobot(i) >= 0) continue;
	world->GetAppearance(i) = originalAppearance[i];
      }
    }
  }

  virtual void Handle_Control(int id)
  {
    switch(id) {
    case SIMULATE_BUTTON_ID:
      if(simulate) {
	simulate=0;
	SleepIdleCallback();
      }
      else {
	simulate=1;
	SleepIdleCallback(0);
      }
      break;
    case RESET_BUTTON_ID:
      simulate = 0;
      ResetSim();
      sim.UpdateRobot(0);
      poseConfig = world->robots[0].robot->q;
      poseGoals.clear();
      currentPoseGoal = -1;
      Refresh();
      break;
    case SAVE_MOVIE_BUTTON_ID:
      //resize for movie
      {
	int totalw = glutGet(GLUT_WINDOW_WIDTH);
	int totalh = glutGet(GLUT_WINDOW_HEIGHT);
	int toolbarw = totalw - viewport.w;
	int toolbarh = totalh - viewport.h;
	glutReshapeWindow(toolbarw+int(settings["movieWidth"]),toolbarh+int(settings["movieHeight"]));
      }
      ToggleMovie();
      break;
    case COMMAND_MILESTONE_BUTTON_ID:
      {
	RobotController* rc=sim.robotControllers[0];
	stringstream ss;
	ss<<poseConfig;
	if(!rc->SendCommand("set_q",ss.str())) {
	  fprintf(stderr,"set_q command does not work with the robot's controller\n");
	}
      }
      break;
    case IO_DATA_CHOICE_ID:
      break;
    case SAVE_ID:
      if(io_choice == IO_SIM_STATE_ID) {
	File f;
	if(!f.Open((char *)file_name.c_str(),FILEWRITE)) {
	  printf("Warning, couldn't open file %s\n",&file_name[0]);
	}
	else {
	  printf("Writing simulation state to %s\n",&file_name[0]);
	  sim.WriteState(f);
	  f.Close();
	}
      }
      else if(io_choice == IO_RAW_COMMANDS_ID) {
	char buf[256];
	for(size_t i=0;i<sim.robotControllers.size();i++) {
	  sprintf(buf,(char *)file_name.c_str(),i);
	  if(sim.robotControllers[i]->SendCommand("log",buf)) 
	    printf("Saved commands to %s\n",buf);
	  else
	    fprintf(stderr,"Error writing to %s\n",buf);
	}
      }
      break;
    case LOAD_ID:
      if(io_choice == IO_SIM_STATE_ID) {
	LoadState(file_name.c_str());
      }
      else if(io_choice == IO_LINEAR_PATH_ID) {
	LoadLinearPath(file_name.c_str());
      }
      else if(io_choice == IO_MILESTONES_ID) {
	LoadMilestones(file_name.c_str());
      }
      else if(io_choice == IO_MULTIPATH_ID) {
	LoadMultiPath(file_name.c_str());
      }
      else if(io_choice == IO_RAW_COMMANDS_ID) {
	char buf[256];
	for(size_t i=0;i<sim.robotControllers.size();i++) {
	  sprintf(buf,(char *)file_name.c_str(),i);
	  if(sim.robotControllers[i]->SendCommand("replay",buf)) 
	    printf("Loaded commands from %s\n",buf);
	  else
	    fprintf(stderr,"Error reading commands from %s\n",buf);
	}
      }
      break;
    case SENSOR_LISTBOX_ID:
      SyncSensorMeasurements();
      break;
    case SENSOR_MEASUREMENT_LISTBOX_ID:
      SyncSensorMeasurementDraw();
      break;
    case DRAW_SENSOR_CHECKBOX_ID:
      SyncSensorMeasurementDrawToCheckbox();
      break;
    case ISOLATE_MEASUREMENT_ID:
      IsolateSensorMeasurementDraw();
      break;
    case DRAW_EXPANDED_CHECKBOX_ID:
      ToggleDrawExpandedCheckbox();
      break;
    case DRIVER_LISTBOX_ID:
      {
	Robot* robot=world->robots[0].robot;
	Vector2 limits=robot->GetDriverLimits(driverValueIndex);
	driver_spinners[driverValueIndex]->set_float_limits(limits.x,limits.y);
	spinner_values[0] = robot->GetDriverValue(driverValueIndex);
	GLUI_Master.sync_live_all();
      }
      break;
    case DRIVER_SPINNER_ID0:
      if(driverValueIndex >= 0) {
	Robot* robot=world->robots[0].robot;
	robot->UpdateConfig(poseConfig);
	robot->SetDriverValue(driverValueIndex,spinner_values[0]);
	poseConfig = robot->q;
	Refresh();
      }
      break;
    case SETTINGS_LISTBOX_ID:
      {
	string value;
	bool res=sim.robotControllers[0]->GetSetting(controllerSettings[controllerSettingIndex],value);
	if(!res) printf("Failed to get setting %s\n",controllerSettings[controllerSettingIndex].c_str());
	else
	  settingEdit->set_text(value.c_str());
      }
    case SETTINGS_ID:
      {
	bool res=sim.robotControllers[0]->SetSetting(controllerSettings[controllerSettingIndex],settingEdit->get_text());
	if(!res) printf("Failed to set setting %s\n",controllerSettings[controllerSettingIndex].c_str());
      }
      break;
    case COMMAND_ID:
      {
	bool res=sim.robotControllers[0]->SendCommand(controllerCommands[controllerCommandIndex],commandEdit->get_text());
	if(!res) printf("Failed to send command %s(%s)\n",controllerCommands[controllerCommandIndex].c_str(),commandEdit->get_text());
      }
      break;
    }
    if(driverValueIndex < 0 && id >= DRIVER_SPINNER_ID0) {
      int i=id-DRIVER_SPINNER_ID0;
      assert(i >= 0 && i <(int)spinner_values.size());
      Robot* robot=world->robots[0].robot;
      robot->UpdateConfig(poseConfig);
      robot->SetDriverValue(i,spinner_values[i]);
      poseConfig = robot->q;
      Refresh();
    }
  }
  
  virtual void BeginDrag(int x,int y,int button,int modifiers)
  {
    if(button == GLUT_RIGHT_BUTTON) {
      if(hoverObject) {
	//TODO: apply force to the object
      }
    }
    else {
      WorldViewProgram::BeginDrag(x,y,button,modifiers);
    }
  }

  virtual void EndDrag(int x,int y,int button,int modifiers)
  {
    if(button == GLUT_RIGHT_BUTTON) {
      if(hoverRobot && hoverLink != -1) {
	currentPoseGoal = -1;
	forceSpringActive = false;
	int index=-1;
	for(size_t i=0;i<world->robots.size();i++)
	  if(&world->robots[i]==hoverRobot) index=(int)i;
	if(index==0) return; //this is controlled by the panel
	if(index >= 0 && tempq.n != 0) {
	  RobotController* rc=sim.robotControllers[index];
	  stringstream ss;
	  ss<<tempq;
	  if(!rc->SendCommand("append_q",ss.str())) {
	    fprintf(stderr,"append_q command does not work with the robot's controller\n");
	  }
	  tempq.clear();
	}
      }
    }
  }

  virtual void DoFreeDrag(int dx,int dy,int button)
  {
    if(button == GLUT_LEFT_BUTTON)  DragRotate(dx,dy);
    else if(button == GLUT_RIGHT_BUTTON) {
      if(hoverRobot && hoverLink != -1) {
	if(forceApplicationMode) {
	  Robot* robot=hoverRobot->robot;
	  Vector3 wp = robot->links[hoverLink].T_World*hoverPt;
	  Vector3 ofs;
	  Vector3 vv;
	  viewport.getViewVector(vv);
	  Vector3 cp,cv;
	  viewport.getClickSource(oldmousex+dx,viewport.h-(oldmousey+dy),cp);
	  viewport.getClickVector(oldmousex+dx,viewport.h-(oldmousey+dy),cv);
	  //vv^T (wp-cp) = vv^T cv*t
	  Real t = vv.dot(wp-cp)/vv.dot(cv);
	  forceSpringAnchor = cp + cv*t;
	  forceSpringActive = true;
	}
	else {
	  Robot* robot=hoverRobot->robot;
	  if(pose_ik) {
	    if(hoverRobot == &world->robots[0]) {
	      robot->UpdateConfig(poseConfig);
	    }
	    else {
	      if(tempq.n == 0) tempq = robot->q;
	      robot->UpdateConfig(tempq);
	      tempq.clear();
	    }
	    if(currentPoseGoal < 0) {
	      for(size_t i=0;i<poseGoals.size();i++) {
		if(poseGoals[i].link == hoverLink) {
		  currentPoseGoal = (int)i;
		  break;
		}
	      }
	      if(currentPoseGoal < 0) {
		poseGoals.resize(poseGoals.size()+1);
		currentPoseGoal = (int)poseGoals.size()-1;
		poseGoals.back().link = hoverLink;
		poseGoals.back().localPosition = hoverPt ;
		poseGoals.back().endPosition = robot->links[hoverLink].T_World*hoverPt;
	      }
	    }
	    IKGoal& goal = poseGoals[currentPoseGoal];
	    //Vector3 wp = robot->links[goal.link].T_World*goal.localPosition;
	    Vector3 wp = goal.endPosition;
	    Vector3 ofs;
	    Vector3 vv;
	    viewport.getViewVector(vv);
	    Real d = (wp-viewport.position()).dot(vv);
	    viewport.getMovementVectorAtDistance(dx,-dy,d,ofs);
	    goal.SetFixedPosition(wp + ofs);

	    int iters=100;
	    bool res=SolveIK(*robot,poseGoals,1e-3,iters,0);
	    if(hoverRobot == &world->robots[0]) {
	      poseConfig = robot->q;
	      SyncDriverSpinners();
	    }
	    tempq = robot->q;
	  }
	  else {
	    if(hoverRobot == &world->robots[0]) {
	      robot->UpdateConfig(poseConfig);
	      for(size_t i=0;i<robot->drivers.size();i++) {
		if(robot->DoesDriverAffect(i,hoverLink)) {
		  Real val = Clamp(robot->GetDriverValue(i)+dy*0.02,robot->drivers[i].qmin,robot->drivers[i].qmax);
		  robot->SetDriverValue(i,val);
		}
	      }
	      poseConfig = robot->q;
	      SyncDriverSpinners();
	    }
	    else {
	      if(tempq.n == 0) tempq = robot->q;
	      robot->UpdateConfig(tempq);
	      for(size_t i=0;i<robot->drivers.size();i++) {
		if(robot->DoesDriverAffect(i,hoverLink)) {
		  Real val = robot->GetDriverValue(i);
		  val = Clamp(val+dy*0.02,robot->drivers[i].qmin,robot->drivers[i].qmax);
		  robot->SetDriverValue(i,val);
		}
	      }
	    }
	    tempq = robot->q;
	  }
	}
	Refresh();
      }
    }
  }

  virtual void DoCtrlDrag(int dx,int dy,int button)
  {
    if(button == GLUT_LEFT_BUTTON)  DragPan(dx,dy);
  }
  
  virtual void DoAltDrag(int dx,int dy,int button)
  {
    if(button == GLUT_LEFT_BUTTON)  DragZoom(dx,dy);
  }

  virtual void DoShiftDrag(int dx,int dy,int button)
  {
    if(button == GLUT_LEFT_BUTTON) { camera.dist *= (1 + 0.01*Real(dy)); }
  }

  void Handle_Motion(int x, int y)
  {
    Ray3D r;
    ClickRay(x,y,r);
    int body;
    Vector3 rlocalpt,olocalpt;
    Real robDist = Inf;
    Real objDist = Inf;

    //check click on the the desired configurations
    for(size_t k=0;k<world->robots.size();k++) {
      Robot* robot=world->robots[k].robot;
      if(k==0 && !forceApplicationMode) {
	robot->UpdateConfig(poseConfig);
      }
      else sim.UpdateRobot(k);
    }
    RobotInfo* rob = ClickRobot(r,body,rlocalpt);
    RigidObjectInfo* obj = ClickObject(r,olocalpt);
    if(rob) 
      robDist = r.closestPointParameter(rob->robot->links[body].T_World*rlocalpt);
    if(obj) 
      objDist = r.closestPointParameter(obj->object->T*olocalpt);
    if(objDist < robDist) rob=NULL;
    else obj=NULL;
    if(rob) {
      if(hoverRobot != rob || hoverLink != body) Refresh();
      hoverRobot = rob;
      hoverLink = body;
      hoverPt = rlocalpt;
    }
    else {
      if(hoverRobot != NULL)  Refresh();
      hoverRobot = NULL;
      hoverLink = -1;
    }
    if(obj) {
      if(hoverObject != obj) Refresh();
      hoverObject = obj;
      hoverPt = olocalpt;
    }
    else {
      if(hoverObject != NULL)  Refresh();
      hoverObject = NULL;
    }
  }

  virtual void Handle_Keypress(unsigned char key,int x,int y)
  {
    switch(key) {
    case 'h':
      printf("Keyboard help:\n");
      printf("[space]: sends the milestone to the controller\n");
      printf("s: toggles simulation\n");
      printf("a: advances the simulation one step\n");
      printf("f: toggles force application mode (right click and drag)\n");
      printf("c: in IK mode, constrains link rotation and position\n");
      printf("d: in IK mode, deletes an ik constraint\n");
      printf("v: save current viewport\n");
      printf("V: load viewport\n");
      break;
    case 'a':
      sim.Advance(sim.simStep);
      if(log_check) DoLogging();
      break;
    case 'f':
      forceApplicationMode = !forceApplicationMode;
      if(forceApplicationMode) {
	printf("Force application mode\n");
      }
      else {
	printf("Joint control mode\n");
      }
      break;
    case 'c':
      if(pose_ik) {
	if(hoverLink < 0) {
	  printf("Before constraining a link you need to hover over it\n");
	}
	else {
	  for(size_t i=0;i<poseGoals.size();i++)
	    if(poseGoals[i].link == hoverLink) {
	      poseGoals.erase(poseGoals.begin()+i);
	      break;
	    }
	  printf("Fixing link %s\n",hoverRobot->robot->LinkName(hoverLink).c_str());
	  poseGoals.resize(poseGoals.size()+1);
	  hoverRobot->robot->UpdateConfig(poseConfig);
	  poseGoals.back().link = hoverLink;
	  poseGoals.back().localPosition = hoverRobot->robot->links[hoverLink].com;
	  poseGoals.back().SetFixedPosition(hoverRobot->robot->links[hoverLink].T_World*hoverRobot->robot->links[hoverLink].com);
	  poseGoals.back().SetFixedRotation(hoverRobot->robot->links[hoverLink].T_World.R);
	}
      }
      break;
    case 'd':
      if(pose_ik) {
	for(size_t i=0;i<poseGoals.size();i++)
	  if(poseGoals[i].link == hoverLink) {
	    printf("Deleting IK goal on link %d\n",hoverLink);
	    poseGoals.erase(poseGoals.begin()+i);
	    break;
	  }
      }
      break;
    case 's':
      if(simulate) {
	simulate=0;
	SleepIdleCallback();
      }
      else {
	simulate=1;
	SleepIdleCallback(0);
      }
      break;
    case ' ':
      {
	Robot* robot=world->robots[0].robot;
	RobotController* rc=sim.robotControllers[0];
	stringstream ss;
	ss<<poseConfig;
	if(!rc->SendCommand("append_q",ss.str())) {
	  fprintf(stderr,"append_q command does not work with the robot's controller\n");
	}
      }
      break;
    case 'v':
      {
	printf("Saving viewport to view.txt\n");
	ofstream out("view.txt",ios::out);
	WriteDisplaySettings(out);
	break;
      }
    case 'V':
      {
	printf("Loading viewport from view.txt...\n");
	ifstream in("view.txt",ios::in);
	if(!in) {
	  printf("Unable to open view.txt\n");
	}
	else {
	  ReadDisplaySettings(in);
	}
	break;
      }
    }
    Refresh();
  }

  void SimStep(Real dt) {
    if(forceSpringActive) {
      assert(hoverRobot && hoverLink != -1);
      int robotIndex = 0;
      //TODO: more than one robot
      Real mass = hoverRobot->robot->GetTotalMass();
      dBodyID body = sim.odesim.robot(robotIndex)->baseBody(hoverLink);
      Vector3 wp = hoverRobot->robot->links[hoverLink].T_World*hoverPt;
      sim.hooks.push_back(new SpringHook(body,wp,forceSpringAnchor,mass));
    }

    sim.Advance(dt);
    if(log_check) DoLogging();
    if(forceSpringActive)
      sim.hooks.resize(sim.hooks.size()-1);
    
    //update root of poseConfig from simulation
    Robot* robot=world->robots[0].robot;
    robot->UpdateConfig(poseConfig);
    Vector driverVals(robot->drivers.size());
    for(size_t i=0;i<robot->drivers.size();i++)
      driverVals(i) = robot->GetDriverValue(i);
    sim.UpdateRobot(0);
    for(size_t i=0;i<robot->drivers.size();i++)
      robot->SetDriverValue(i,driverVals(i));
    poseConfig = robot->q;
  }

  void SensorPlotUpdate()
  {
    RobotSensors& sensors = sim.controlSimulators[0].sensors;
    SensorBase* sensor = NULL;
    sensor = sensors.sensors[sensorSelectIndex];

    sensorPlot.xmin = sim.time - Real(settings["sensorPlot"]["duration"]);
    sensorPlot.xmax = sim.time;

    vector<double> values;
    sensor->GetMeasurements(values);
    for(size_t i=0;i<values.size();i++) {
      sensorPlot.AddPoint(sim.time,values[i],i);
    }
  }

  virtual void Handle_Idle() {
    if(simulate) {
      double dt=settings["updateStep"];
      Timer timer;
      SimStep(dt);
      Refresh();

      SensorPlotUpdate();

      MovieUpdate(sim.time);
      if(!saveMovie) SleepIdleCallback(int(Max(0.0,dt-timer.ElapsedTime())*1000.0));
    }
    WorldViewProgram::Handle_Idle();
  }
};

int main(int argc, const char** argv)
{ 
  if(argc < 2) {
    printf("USAGE: SimTest [options] [robot, terrain, object, world files]\n");
    return 0;
  }
  RobotWorld world;
  SimTestProgram program(&world);
  if(!program.LoadAndInitSim(argc,argv)) {
    return 1;
  }
  return program.Run();
}

