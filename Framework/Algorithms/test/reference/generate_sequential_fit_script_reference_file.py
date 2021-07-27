# A python script generated to perform a sequential or simultaneous fit
from mantid.simpleapi import *
import matplotlib.pyplot as plt

# Dictionary { workspace_name: (workspace_index, start_x, end_x) }
input_data = {
    "Name1": (0, 0.500000, 1.500000),
    "Name2": (1, 0.600000, 1.600000)
}

# Fit function as a string
function = \
    "name=GausOsc,A=0.2,Sigma=0.2,Frequency=0.1,Phi=0"

# Fitting options
max_iterations = 500
minimizer = "Levenberg-Marquardt"
cost_function = "Least squares"
evaluation_type = "CentrePoint"

# Perform a sequential fit
output_workspaces, parameter_tables, normalised_matrices = [], [], []
for input_workspace, domain_data in input_data.items():
    fit_output = Fit(Function=function, InputWorkspace=input_workspace, WorkspaceIndex=domain_data[0],
                     StartX=domain_data[1], EndX=domain_data[2], MaxIterations=max_iterations,
                     Minimizer=minimizer, CostFunction=cost_function, EvaluationType=evaluation_type,
                     CreateOutput=True)

    output_workspaces.append(fit_output.OutputWorkspace)
    parameter_tables.append(fit_output.OutputParameters)
    normalised_matrices.append(fit_output.OutputNormalisedCovarianceMatrix)

    # Use the parameters in the previous function as the start parameters of the next fit
    function = fit_output.Function

# Group the output workspaces from the sequential fit
GroupWorkspaces(InputWorkspaces=output_workspaces, OutputWorkspace="Sequential_Fit_Workspaces")
GroupWorkspaces(InputWorkspaces=parameter_tables, OutputWorkspace="Sequential_Fit_Parameters")
GroupWorkspaces(InputWorkspaces=normalised_matrices, OutputWorkspace="Sequential_Fit_NormalisedCovarianceMatrices")

# Plot the results of the fit
fig, axes = plt.subplots(nrows=2,
                         ncols=len(output_workspaces),
                         sharex=True,
                         gridspec_kw={"height_ratios": [2, 1]},
                         subplot_kw={"projection": "mantid"})

for i, workspace in enumerate(output_workspaces):
    axes[0, i].errorbar(workspace, "rs", wkspIndex=0, label="Data", markersize=2)
    axes[0, i].errorbar(workspace, "b-", wkspIndex=1, label="Fit")
    axes[0, i].set_title(workspace.name())
    axes[0, i].set_xlabel("")
    axes[0, i].tick_params(axis="both", direction="in")
    axes[0, i].legend()

    axes[1, i].errorbar(workspace, "ko", wkspIndex=2, markersize=2)
    axes[1, i].set_ylabel("Difference")
    axes[1, i].tick_params(axis="both", direction="in")

fig.subplots_adjust(hspace=0)
fig.show()
