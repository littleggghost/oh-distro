from time import time
import numpy as np
from ddapp import robotstate
from ddapp import transformUtils
from ddapp import visualization as vis
from ddapp import segmentation
from ddapp.plansequence import RobotPoseGUIWrapper

from copy import deepcopy

# Declare all valid inputs/ouputs here
# Using a dictionary to possibly give them type information later
# right now the keys of this dict are the valid list of arguments
argType = {}
argType['Affordance'] = None
argType['WalkPlan'] = None
argType['WalkTarget'] = None
argType['RobotPose'] = None
argType['JointPlan'] = None
argType['Hand'] = None
argType['Constraints'] = None


# Base Class, all actions must inherit from this
# All inheritors must:
#
# In constructor, need to pass the following
# -instance name
# -name of an instance to transition to on success
# -name of an instance to transition to on failure
# -a list of arguments passed in
# -a reference to an ActionSequence object used as a data container
#  to share data between Actions
#
# Inputs:
# -a list of arguments that must be present in the args dict
#
# Outputs:
# -dictonary to store the outputs of an action
# -starts empty (keys indicate valid outputs)
# -after running, will be populated with output data
#

class Action(object):

    inputs = []

    def __init__(self, name, success, fail, args, container):
        self.name = name
        self.container = container
        self.args = args     # storage for raw input arguments
        self.parsedArgs = {} # storage for inputs after being processed
        self.successAction = success
        self.failAction = fail
        self.animations = []
        self.inputState = None
        self.outputState = None

    def onEnter(self):
        print "default enter"

    def onUpdate(self):
        print "default update"

    def onExit(self):
        print "default exit"

    def transition(self, val):
        self.container.fsm.transition(val)

    def success(self):
        self.transition(self.successAction)
        self.container.executionList.append([self.name, 'success'])

    def fail(self):
        self.transition(self.failAction)
        self.container.executionList.append([self.name, 'fail'])

    def hardFail(self):
        self.transition('fail')
        self.container.executionList.append([self.name, 'abort'])

    def argParseAndEnter(self):
        if self.container.previousAction != None and self.container.previousAction.outputState != None:
            self.inputState = self.container.previousAction.outputState
        else:
            print "ERROR: no previous state found, using estimated current state"
            self.inputState = self.container.sensorJointController.getPose('EST_ROBOT_STATE')
        self.parseInputs()
        self.onEnter()

    def storeEndStateAndExit(self):
        self.container.previousAction = self
        self.onExit()

    def checkInputArgs(self, args, inputs):
        # Simple arg safety checking

        # Do we have the right number of args?
        if not len(self.inputs) == len(self.args.keys()):
            return False

        # Does every arg also exist in the input list?
        valid = True
        for arg in args:
            if arg not in inputs:
                valid = False
                print "Arg:", arg, "provided but it is not a valid input for:", self.name

        return valid

    def parseInputs(self):
        # args dictionary provides all the inputs, but these can be of two forms:
        # strings: indicating an specified input
        # or
        # references to other action classes: indicating intention to get the
        # input from that classes output at execution time
        #
        # Call this function at runtime to check consitency and grab
        # the output args from the specificed classes

        if not self.checkInputArgs(self.args, self.inputs):
            self.hardFail()
            return

        for arg in self.args.keys():
            if isinstance(self.args[arg], str):
                self.parsedArgs[arg] = self.args[arg]
            elif isinstance(self.args[arg], Action):
                self.parsedArgs[arg] = deepcopy(self.args[arg].outputs[arg])
            else:
                print "ERROR: provided input arg is neither a string nor a class reference"
                self.parsedArgs[arg] = None
                self.hardFail()

    def animate(self, index):
        if self.animations != []:
            self.container.playbackFunction([self.animations[index]])
        else:
            print "ERROR: This action does not have an animation"

# Below are standard success and fail actions which simply terminate
# the sequence with a success or fail message

class Goal(Action):

    inputs = []

    def __init__(self, container):
        Action.__init__(self, 'goal', None, None, {}, container)
        self.outputs = {}

    def onEnter(self):
        print "HOORAY! Entered the GOAL state"

        # If in vizmode, play the animation
        if self.container.vizMode:
            self.container.play()

        # Stop the FSM now that we're at a terminus
        self.container.fsm.stop()


class Fail(Action):

    inputs = []

    def __init__(self, container):
        Action.__init__(self, 'fail', None, None, {}, container)
        self.outputs = {}

    def onEnter(self):
        print "NOOO! Entered the FAIL state"

        # Stop the FSM now that we're at a terminus
        self.container.fsm.stop()


# Full action objects start below:
#
# Generic notes:
#        state transition when success
#        state transition when fail
#        function to evaluate
#        on enter function -kick off eval function
#        on update function -wait for success fail
#        on exit function -teardown, if necessary

class ChangeMode(Action):

    inputs = ['NewMode']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}
        self.timeOut = 10.0
        self.enterTime = 0.0

    def onEnter(self):
        # Logic for viz-mode:
        if self.container.vizMode:
            self.currentBehavior = self.parsedArgs['NewMode']

        # Logic for execute mode:
        else:
            self.enterTime = time()
            self.behaviorTarget = self.parsedArgs['NewMode']
            self.container.atlasDriver.sendBehaviorCommand(self.behaviorTarget)

    def onUpdate(self):
        # Logic for viz-mide:
        if self.container.vizMode:
            self.success()

        # Logic for execute mode:
        else:
            # Error checking on bad mode types
            if self.behaviorTarget not in self.container.atlasDriver.getBehaviorMap().values():
                print 'ERROR: desired transition does not exist, failing'
                self.fail()
            else:
                self.currentBehavior = self.container.atlasDriver.getCurrentBehaviorName()

                if self.currentBehavior == self.behaviorTarget:
                    self.success()
                else:
                    if time() > self.enterTime + self.timeOut:
                        print 'ERROR: mode change timed out'
                        self.fail()
                    else:
                        return

    def onExit(self):

        # This is a state change action, no robot motion
        self.outputState = deepcopy(self.inputState)

class WalkPlan(Action):

    inputs = ['WalkTarget']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'WalkPlan' : None}
        self.walkPlanResponse = None

    def onEnter(self):
        graspStanceFrame = self.container.om.findObjectByName(self.parsedArgs['WalkTarget'])
        self.walkPlanResponse = self.container.footstepPlanner.sendFootstepPlanRequest(graspStanceFrame.transform, waitForResponse=True, waitTimeout=0)
        # TODO: this only plans from current state, need to fix that

    def onUpdate(self):

        response = self.walkPlanResponse.waitForResponse(timeout = 0)

        if response:
            if response.num_steps == 0:
                print 'walk plan failed'
                self.fail()
            else:
                print 'walk plan successful'
                self.container.footstepPlan = response
                self.outputs['WalkPlan'] = response
                self.success()

    def onExit(self):
        # Cleanup planner objects
        self.walkPlanResponse.finish()
        self.walkPlanResponse = None

        # This is a planning action, no robot motion
        self.outputState = deepcopy(self.inputState)

class Walk(Action):

    inputs = ['WalkPlan']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}
        self.walkAnimationResponse = None

    def onEnter(self):

        # Logic for viz-mode:
        if self.container.vizMode:
            # Use the footstep plan to calculate a walking plan, for animation purposes only
            self.walkAnimationResponse = self.container.footstepPlanner.sendWalkingPlanRequest(self.container.footstepPlan, waitForResponse=True, waitTimeout=0)

        # Logic for execute mode:
        else:
            self.container.footstepPlanner.commitFootstepPlan(self.container.footstepPlan)
            self.walkStart = False

    def onUpdate(self):

        # Logic for viz-mode:
        if self.container.vizMode:
            response = self.walkAnimationResponse.waitForResponse(timeout = 0)
            if response:
                # We got a successful response from the walk planner, cache it and update the plan state
                self.animations.append(response)
                self.container.vizModeAnimation.append(response)
                self.success()

        # Logic for execute mode:
        else:
            if self.container.atlasDriver.getCurrentBehaviorName() == 'step':
                self.walkStart = True
            if self.walkStart and self.container.atlasDriver.getCurrentBehaviorName() == 'stand':
                self.success()

    def onExit(self):
        # Cleanup planner code
        # Logic for viz-mode:
        if self.container.vizMode:
            self.walkAnimationResponse.finish()
            self.walkAnimationResponse = None

        # Logic for execute-mode:
        else:
            self.walkStart = None

        # This is a motion action, output should be new robot state, based on mode selection
        if self.container.vizMode:
            # Viz Mode Logic
            # (simulating a perfect execution, output state is the end of animation state)
            self.outputState = robotstate.convertStateMessageToDrakePose(self.animations[-1].plan[-1])
        else:
            # Execute Mode Logic
            # (the move is complete, so output state is current estimated robot state)
            self.outputState = self.container.sensorJointController.getPose('EST_ROBOT_STATE')
        return

def computeGroundFrame(robotModel):
    '''
    Given a robol model, returns a vtkTransform at a position between
    the feet, on the ground, with z-axis up and x-axis aligned with the
    robot pelvis x-axis.
    '''
    t1 = robotModel.getLinkFrame('l_foot')
    t2 = robotModel.getLinkFrame('r_foot')
    pelvisT = robotModel.getLinkFrame('pelvis')

    xaxis = [1.0, 0.0, 0.0]
    pelvisT.TransformVector(xaxis, xaxis)
    xaxis = np.array(xaxis)
    zaxis = np.array([0.0, 0.0, 1.0])
    yaxis = np.cross(zaxis, xaxis)
    yaxis /= np.linalg.norm(yaxis)
    xaxis = np.cross(yaxis, zaxis)

    stancePosition = (np.array(t2.GetPosition()) + np.array(t1.GetPosition())) / 2.0

    footHeight = 0.0811

    t = transformUtils.getTransformFromAxes(xaxis, yaxis, zaxis)
    t.PostMultiply()
    t.Translate(stancePosition)
    t.Translate([0.0, 0.0, -footHeight])

    return t

class PoseSearch(Action):

    inputs = ['Affordance', 'Hand']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'TargetFrame' : None, 'WalkTarget' : None, 'Hand' : None}

    def onEnter(self):

        self.targetAffordance = self.container.om.findObjectByName(self.parsedArgs['Affordance'])
        self.targetAffordanceFrame = self.container.om.findObjectByName(self.parsedArgs['Affordance'] + ' frame')

        self.outputs['TargetFrame'] = self.parsedArgs['Affordance'] + ' grasp frame'
        self.outputs['WalkTarget'] = self.parsedArgs['Affordance'] + ' stance frame'
        self.outputs['Hand'] = self.parsedArgs['Hand']

        #Calculate where to place the hand (hard coded offsets based on drill, just for testing)
        # for left_base_link
        position = [-0.12, 0.0, 0.028]
        if self.parsedArgs['Hand'] == 'right':
            rpy = [180, 0, 90]
        else:
            rpy = [0, 0, -90]

        grasp = transformUtils.frameFromPositionAndRPY(position, rpy)
        grasp.Concatenate(self.targetAffordanceFrame.transform)
        self.graspFrame = vis.updateFrame(grasp, self.outputs['TargetFrame'], parent=self.targetAffordance, visible=True, scale=0.25)

        #Calculate where to place the walking target (hard coded offsets based on drill, just for testing)
        graspFrame = self.graspFrame.transform
        groundFrame = computeGroundFrame(self.container.robotModel)
        groundHeight = groundFrame.GetPosition()[2]

        graspPosition = np.array(graspFrame.GetPosition())
        graspYAxis = [0.0, 1.0, 0.0]
        graspXAxis = [1.0, 0.0, 0.0]
        graspFrame.TransformVector(graspYAxis, graspYAxis)
        graspFrame.TransformVector(graspXAxis, graspXAxis)

        xaxis = graspYAxis
        zaxis = [0, 0, 1]
        yaxis = np.cross(zaxis, xaxis)
        yaxis /= np.linalg.norm(yaxis)
        graspGroundFrame = transformUtils.getTransformFromAxes(xaxis, yaxis, zaxis)
        graspGroundFrame.PostMultiply()
        graspGroundFrame.Translate(graspPosition[0], graspPosition[1], groundHeight)

        if self.parsedArgs['Hand'] == 'right':
            position = [-0.55, 0.50, 0.0]
        else:
            position = [-0.55, -0.50, 0.0]
        rpy = [0, 0, 0]

        stance = transformUtils.frameFromPositionAndRPY(position, rpy)
        stance.Concatenate(graspGroundFrame)

        self.stanceFrame = vis.updateFrame(stance, self.outputs['WalkTarget'], parent=self.targetAffordance, visible=True, scale=0.25)

        return

    def onUpdate(self):
        self.success()

    def onExit(self):
        # This is a planning action, no robot motion
        self.outputState = deepcopy(self.inputState)


class DeltaReachPlan(Action):

    inputs = ['TargetFrame', 'Hand', 'Style', 'Direction', 'Amount']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'JointPlan' : None}
        self.manipPlanResponse = None

    def onEnter(self):

        linkMap = { 'left' : 'l_hand', 'right': 'r_hand'}
        linkName = linkMap[self.parsedArgs['Hand']]

        graspFrame = self.container.om.findObjectByName(self.parsedArgs['TargetFrame'])
        deltaFrame = transformUtils.frameFromPositionAndRPY([0,0,0],[0,0,0])

        vis.updateFrame(deltaFrame, self.parsedArgs['TargetFrame'] + " " + self.name, parent=graspFrame, visible=True, scale=0.25)

        deltaFrame.DeepCopy(graspFrame.transform)

        if self.parsedArgs['Direction'] == 'X':
            delta = transformUtils.frameFromPositionAndRPY([float(self.parsedArgs['Amount']),0,0],[0,0,0])
        elif self.parsedArgs['Direction'] == 'Y':
            delta = transformUtils.frameFromPositionAndRPY([0,float(self.parsedArgs['Amount']),0],[0,0,0])
        else:
            delta = transformUtils.frameFromPositionAndRPY([0,0,float(self.parsedArgs['Amount'])],[0,0,0])

        if self.parsedArgs['Style'] == 'Local':
            deltaFrame.PreMultiply()
        else:
            deltaFrame.PostMultiply()
        deltaFrame.Concatenate(delta)

        self.manipPlanResponse = self.container.manipPlanner.sendEndEffectorGoal(self.inputState, linkName, deltaFrame, waitForResponse=True, waitTimeout=0)

    def onUpdate(self):
        response = self.manipPlanResponse.waitForResponse(timeout = 0)

        if response:
            if response.plan_info[-1] > 10:
                print "PLANNER REPORTS ERROR!"

                # Plan failed, save it in the animation list, but don't update output data
                self.animations.append(response)

                self.fail()
            else:
                print "Planner reports success!"

                # Plan was successful, save it to be animated and update output data
                self.animations.append(response)
                self.outputs['JointPlan'] = deepcopy(response)

                self.success()

    def onExit(self):
        # Planner Cleanup
        self.manipPlanResponse.finish()
        self.manipPlanResponse = None

        # This is a planning action, no robot motion
        self.outputState = deepcopy(self.inputState)

class ReachPlan(Action):

    inputs = ['TargetFrame', 'Hand', 'Constraints']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'JointPlan' : None}
        self.manipPlanResponse = None

    def onEnter(self):
        linkMap = { 'left' : 'l_hand', 'right': 'r_hand'}

        linkName = linkMap[self.parsedArgs['Hand']]
        graspFrame = self.container.om.findObjectByName(self.parsedArgs['TargetFrame'])

        self.manipPlanResponse = self.container.manipPlanner.sendEndEffectorGoal(self.inputState, linkName, graspFrame.transform, waitForResponse=True, waitTimeout=0)

    def onUpdate(self):
        response = self.manipPlanResponse.waitForResponse(timeout = 0)

        if response:
            if response.plan_info[-1] > 10:
                print "PLANNER REPORTS ERROR!"

                # Plan failed, save it in the animation list, but don't update output data
                self.animations.append(response)

                self.fail()
            else:
                print "Planner reports success!"

                # Plan was successful, save it to be animated and update output data
                self.animations.append(response)
                self.outputs['JointPlan'] = deepcopy(response)

                self.success()

    def onExit(self):
        # Planner Cleanup
        self.manipPlanResponse.finish()
        self.manipPlanResponse = None

        # This is a planning action, no robot motion
        self.outputState = deepcopy(self.inputState)

class JointMovePlan(Action):

    inputs = ['PoseName', 'Group', 'Hand']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'JointPlan' : None}
        self.jointMovePlanResponse = None

    def onEnter(self):
        # Send a planner request
        goalPoseJoints = RobotPoseGUIWrapper.getPose(self.parsedArgs['Group'], self.parsedArgs['PoseName'], self.parsedArgs['Hand'])
        self.jointMovePlanResponse = self.container.manipPlanner.sendPoseGoal(self.inputState, goalPoseJoints, waitForResponse=True, waitTimeout=0)

    def onUpdate(self):
        # Wait for planner response
        response = self.jointMovePlanResponse.waitForResponse(timeout = 0)
        if response:
            self.animations.append(response)
            self.outputs['JointPlan'] = response
            self.success()

    def onExit(self):
        #This is a planning action, no robot motion
        self.outputState = deepcopy(self.inputState)


class JointMove(Action):

    inputs = ['JointPlan']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {'RobotPose' : None}
        self.startTime = 0.0

    def onEnter(self):
        #Create an animation based on the incoming plan
        self.animations.append(self.parsedArgs['JointPlan'])
        self.container.vizModeAnimation.append(self.parsedArgs['JointPlan'])

        if self.container.vizMode:
            # Viz Mode Logic
            return
        else:
            # Execute Mode Logic
            # Send the command to the robot
            self.container.manipPlanner.commitManipPlan(self.parsedArgs['JointPlan'])
            self.startTime = time()
            return

    def onUpdate(self):
        #Perform the motion
        if self.container.vizMode:
            # Viz Mode Logic
            # do nothing
            self.success()
        else:
            # Execute Mode Logic
            # Wait for success
            if time() > self.startTime + 10.0:

                # Need logic here to see if we reached our target to within some tolerance
                # success or fail based on that
                # is there a way to see if the robot is running?
                self.success()

    def onExit(self):
        # This is a motion action, output should be new robot state, based on mode selection
        if self.container.vizMode:
            # Viz Mode Logic
            # (simulating a perfect execution, output state is the end of animation state)
            self.outputState = robotstate.convertStateMessageToDrakePose(self.animations[-1].plan[-1])
        else:
            # Execute Mode Logic
            # (the move is complete, so output state is current estimated robot state)
            self.outputState = self.container.sensorJointController.getPose('EST_ROBOT_STATE')


class Grip(Action):

    inputs = ['Hand']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}
        self.gripTime = 0.0
        self.gripWait = 2.0

    def onEnter(self):
        if self.container.vizMode:
            # Viz Mode Logic
            # Do nothing
            return
        else:
            # Execute Mode Logic
            self.container.handDriver.sendClose(100)
            self.gripTime = time()

    def onUpdate(self):
        if self.container.vizMode:
            # Viz Mode Logic
            self.success()
        else:
            # Execute Mode Logic
            # Wait for grip time, then send another command just to ensure
            # proper regrasp
            if time() > self.gripTime + self.gripWait:
                self.container.handDriver.sendClose(100)
                self.success()

    def onExit(self):
        # This action does not move the robot, set the output state accordingly
        # TODO: make this animate the fingers in the future
        self.outputState = deepcopy(self.inputState)


class Release(Action):

    inputs = ['Hand']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}
        self.gripTime = 0.0
        self.gripWait = 1.0

    def onEnter(self):
        if self.container.vizMode:
            # Viz Mode Logic
            # Do nothing
            return
        else:
            # Execute Mode Logic
            self.container.handDriver.sendOpen()
            self.gripTime = time()

    def onUpdate(self):
        if self.container.vizMode:
            # Viz Mode Logic
            self.success()
        else:
            # Execute Mode Logic
            # Wait for grip time, then send another command just to ensure
            # proper regrasp
            if time() > self.gripTime + self.gripWait:
                self.container.handDriver.sendOpen()
                self.success()

    def onExit(self):
        # This action does not move the robot, set the output state accordingly
        # TODO: make this animate the fingers in the future
        self.outputState = deepcopy(self.inputState)


class Fit(Action):

    inputs = ['Affordance']

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}

    def onEnter(self):
        return

    def onUpdate(self):
        if self.container.vizMode:
            # Viz Mode Logic
            self.success()
        else:
            # Execute Mode Logic
            pd = self.container.om.findObjectByName('Multisense').model.revPolyData
            self.container.om.removeFromObjectModel(self.container.om.findObjectByName('debug'))
            segmentation.findAndFitDrillBarrel(pd, self.container.robotModel.getLinkFrame('utorso'))
            self.success()

    def onExit(self):
        # This action does not move the robot, set the output state accordingly
        # TODO: make this animate the fingers in the future
        self.outputState = deepcopy(self.inputState)


class WaitForScan(Action):

    inputs = []

    def __init__(self, name, success, fail, args, container):
        Action.__init__(self, name, success, fail, args, container)
        self.outputs = {}

    def onEnter(self):

        # Viz Mode Logic
        if self.container.vizMode:
            return

        # Execute Mode Logic
        else:
            self.currentRevolution = self.container.multisenseDriver.displayedRevolution
            self.desiredRevolution = self.currentRevolution + 2

    def onUpdate(self):

        # Viz Mode Logic
        if self.container.vizMode:
            self.success()

        # Execute Mode Logic
        else:
            if self.container.multisenseDriver.displayedRevolution >= self.desiredRevolution:
                self.success()

    def onExit(self):
        # Cleanup
        self.currentRevolution = None
        self.desiredRevolution = None

        # This action does not move the robot, set the output state accordingly
        # TODO: make this animate the fingers in the future
        self.outputState = deepcopy(self.inputState)
