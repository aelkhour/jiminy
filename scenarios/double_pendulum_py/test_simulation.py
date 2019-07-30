import os
import time
import numpy as np
import matplotlib.pyplot as plt
import numba as nb

import pinocchio as pnc
import jiminy
from jiminy_py import *

################################## User parameters #######################################

urdf_path = os.path.join(os.environ["HOME"], "wdc_workspace/src/jiminy/data/double_pendulum/double_pendulum.urdf")

############################# Initialize the simulation #################################

# Instantiate the model
contacts = []
motors = ["SecondPendulumJoint"]
model = jiminy.model()
model.initialize(urdf_path, contacts, motors, False)

# Instantiate the controller
@nb.jit(nopython=True, nogil=True)
def computeCommand(t, q, v, u):
    u[0] = 0.0
@nb.jit(nopython=True, nogil=True)
def internalDynamics(t, q, v, u):
    u[:] = 0.0
controller = jiminy.controller_functor(computeCommand, internalDynamics)
controller.initialize(model)

# Instantiate the engine
simulator = jiminy.simulator(model, controller)

########################### Configuration the simulation ################################

model_options = model.get_model_options()
sensors_options = model.get_sensors_options()
simu_options = simulator.get_engine_options()
ctrl_options = controller.get_controller_options()

model_options["telemetry"]["enableImuSensors"] = True
simu_options["telemetry"]["enableConfiguration"] = True
simu_options["telemetry"]["enableVelocity"] = True
simu_options["telemetry"]["enableAcceleration"] = True
simu_options["telemetry"]["enableCommand"] = True
simu_options["telemetry"]["enableEnergy"] = True
simu_options["world"]["gravity"][2] = -9.81
simu_options["stepper"]["solver"] = "runge_kutta_dopri5" # ["runge_kutta_dopri5", "explicit_euler"]
simu_options["stepper"]["tolRel"] = 1.0e-5
simu_options["stepper"]["tolAbs"] = 1.0e-4
simu_options["stepper"]["dtMax"] = 2.0e-3 # 2.0e-4 for "explicit_euler", 3.0e-3 for "runge_kutta_dopri5"
simu_options["stepper"]["iterMax"] = 100000
simu_options["stepper"]["sensorsUpdatePeriod"] = 1.0e-3
simu_options["stepper"]["controllerUpdatePeriod"] = 1.0e-3
simu_options["stepper"]["randomSeed"] = 0
simu_options['contacts']['stiffness'] = 1.0e6
simu_options['contacts']['damping'] = 2000.0
simu_options['contacts']['dryFrictionVelEps'] = 0.01
simu_options['contacts']['frictionDry'] = 5.0
simu_options['contacts']['frictionViscous'] = 5.0
simu_options['contacts']['transitionEps'] = 0.001

model.set_model_options(model_options)
model.set_sensors_options(sensors_options)
simulator.set_engine_options(simu_options)
controller.set_controller_options(ctrl_options)

################################ Run the simulation #####################################

x0 = np.zeros((4,1))
x0[1] = 0.1
tf = 3.0

simulator.run(x0, 5e-2) # Force compile Python controller for a fair benchmark

start = time.time()
simulator.run(x0, tf)
end = time.time()
print("Simulation time: %03.0fms" %((end - start)*1.0e3))

############################### Extract the results #####################################

log_info, log_data = simulator.get_log()
log_info = list(log_info)
log_data = np.asarray(log_data)
log_constants = log_info[1:log_info.index('StartColumns')]
log_header = log_info[(log_info.index('StartColumns')+1):-1]

print('%i log points' % log_data.shape[0])
print(log_constants)
trajectory_data_log = extract_state_from_simulation_log(
    log_header, log_data, urdf_path, model.pinocchio_model, False)

# Save the log in CSV
# simulator.write_log("/tmp/blackbox/log.data", False)

############################## Display the results ######################################

# Plot some data using standard tools only
# plt.plot(log_data[:,log_header.index('Global.Time')],
#          log_data[:,log_header.index('HighLevelController.energy')])
# plt.show()

# Display the simulation trajectory and the reference
# play_trajectories([trajectory_data_log], speed_ratio=0.5)