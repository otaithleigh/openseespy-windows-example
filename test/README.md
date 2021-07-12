Contents
========

- `test_frictionSpringDamper.(py|html)`
  - Test script to make sure `frictionSpringDamper` works. Requires `bokeh`,
    which comes with Anaconda.
- `uniaxialmaterialanalysis.py`
  - The class used for `test_frictionSpringDamper.py`. Standalone, may come in
    handy.


`uniaxialmaterialanalysis.py`
-----------------------------

Self-contained OpenSeesPy uniaxial material tester module. Requires NumPy and
OpenSeesPy.

### Usage

Define a function that creates the material in OpenSeesPy when called, and
returns the material's tag:

``` python
import opensees as ops

def material():
    ops.uniaxialMaterial('ElasticPP', 1, 10000.0, 75.0)
    return 1
```

Create an analysis object with that function:

``` python
from uniaxialmaterialanalysis import UniaxialMaterialAnalysis

analysis = UniaxialMaterialAnalysis(material)
```

Run the analysis by passing the strain peaks, and an optional interpolation
method for those peaks:

``` python
results = analysis.runAnalysis([0, 100, -100, 200, -200, 0], strainRate=0.1)
# OR
results = analysis.runAnalysis([0, 100, -100, 200, -200, 0], numSteps=1000)
```
