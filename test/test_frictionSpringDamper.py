import bokeh.io
import bokeh.plotting

import opensees as ops
from uniaxialmaterialanalysis import UniaxialMaterialAnalysis


#===============================================================================
# Create and run analysis
#===============================================================================
def material():
    ops.uniaxialMaterial('frictionSpringDamper', 1, 1000, 50, 25, 60)
    return 1


analysis = UniaxialMaterialAnalysis(material)

results = analysis.runAnalysis([0, 100, -100, 200, -200, 300, -300, 0],
                               strainRate=0.1)

#===============================================================================
# Figure
#===============================================================================
bokeh.io.output_file('test_frictionSpringDamper.html')

fig = bokeh.plotting.figure(x_axis_label='Strain', y_axis_label='Stress')
fig.line(results['strain'], results['stress'])

bokeh.plotting.show(fig)
