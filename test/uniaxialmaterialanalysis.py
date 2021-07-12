from __future__ import annotations

import logging
import pathlib
import tempfile
import typing as t

import numpy as np
import opensees as ops

__all__ = [
    'UniaxialMaterialAnalysis',
]

#===============================================================================
# Uniaxial material analysis
#===============================================================================
class OpenSeesAnalysis():
    def __init__(self, scratchPath=None, analysisID=None):
        self.logger = getClassLogger(self)
        self.scratchFile = scratchFileFactory(self.__class__.__name__,
                                              scratchPath, analysisID)
        self.deleteFiles = True


class UniaxialMaterialAnalysis(OpenSeesAnalysis):
    def __init__(
            self,
            materialFactory: t.FunctionType,
            matTag: int = 1,
            scratchPath=None,
            analysisID=None,
            test: str = 'NormUnbalance',
            test_tolerance: float = 1e-8,
            test_maxiters: int = 10,
            algorithm: str = 'Newton',
            constraints: str = 'Transformation',
            system: str = 'UmfPack',
            numberer: str = 'RCM',
        ):
        self.materialFactory = materialFactory
        self.matTag = matTag
        super().__init__(scratchPath=scratchPath, analysisID=analysisID)

        # Default settings, tweakable
        self.test = test
        self.test_tolerance = test_tolerance
        self.test_maxiters = test_maxiters
        self.algorithm = algorithm

        # Analysis settings that are tweakable, but probably don't need to be
        self.constraints = constraints
        self.system = system
        self.numberer = numberer

    def runAnalysis(self,
                    peaks,
                    *,
                    numSteps: int = None,
                    strainRate: float = None):
        """
        Parameters
        ----------
        peaks : array_like
            1-D array of imposed displacement targets.

        Keyword-only parameters
        -----------------------
        numSteps : int, optional
            The number of additional points to interpolate between points in
            `peaks`.
        strainRate : float, optional
            The fixed strain rate (per step) to use to interpolate between
            points in `peaks`.
        
        It is an error to specify both `numSteps` and `strainRate`. If neither
        are specified, no interpolation between points in `peaks` is performed.
        """
        # Check arguments
        if numSteps is not None and strainRate is not None:
            raise ValueError('cannot specify both `numSteps` and `strainRate`')

        peaks: np.ndarray = np.asarray(peaks)
        if peaks.ndim != 1:
            raise ValueError('`peaks` must be a 1-dimensional array')

        # Fill between peaks
        if numSteps is not None:
            values = stepsFill(peaks, numSteps)
        elif strainRate is not None:
            values = strainRateFill(peaks, strainRate)
        else:
            values = peaks

        # Convert to list of Python floats to make OpenSeesPy happy
        values = values.tolist()

        #-------------------------------
        # Create model
        #-------------------------------
        ops.wipe()
        ops.model('basic', '-ndm', 1, '-ndf', 1)

        # Define Nodes
        ops.node(1, 0.0)
        ops.node(2, 1.0)

        # Define Boundary Conditions
        ops.fix(1, 1)

        # Define Elements
        self.materialFactory()
        ops.element('Truss', 1, 1, 2, 1.0, self.matTag)

        # Define Loads
        ops.timeSeries('Path', 854, '-dt', 1.0, '-values', *values, '-useLast',
                       '-prependZero')
        ops.pattern('Plain', 979, 854)
        ops.sp(2, 1, 1.0)

        #-------------------------------
        # Build and run the analysis
        #-------------------------------
        ops.constraints('Transformation')
        ops.numberer('RCM')
        ops.system('UmfPack')
        ops.test('NormUnbalance', 1e-8, 10)
        ops.algorithm('Newton')
        ops.integrator('LoadControl', 1)
        ops.analysis('Static')

        stress = []
        strain = []

        def record():
            stress.append(*ops.eleResponse(1, 'axialForce'))
            strain.append(*ops.eleResponse(1, 'deformations'))

        for i in range(len(values)):
            status = ops.analyze(1)
            if status != 0:
                self.logger.error('Analysis failed to converge at step %d.', i)
                break
            record()

        results = {
            'stress': np.array(stress),
            'strain': np.array(strain),
        }
        return results


def strainRateFill(peakPoints: np.ndarray, rate: float):
    return fillOutNumbers(peakPoints, rate)


def stepsFill(peakPoints: np.ndarray, numSteps: int) -> np.ndarray:
    return np.concatenate([
        *(np.linspace(i, j, numSteps, endpoint=False)
          for i, j in zip(peakPoints, peakPoints[1:])),
        [peakPoints[-1]],
    ])


#===============================================================================
# Utilities
#===============================================================================
def getClassLogger(o) -> logging.Logger:
    """Get a logger scoped to the class of an object.

    Parameters
    ----------
    o : object
        Object to get a logger for.

    Example
    -------
    >>> class ClassWithLogger():
    ...     def __init__(self):
    ...         self.logger = getClassLogger(self)
    ...
    ...     def some_func(self, msg):
    ...         self.logger.warning(msg)
    ...
    >>> logging.basicConfig(format='%(name)s.%(funcName)s: %(message)s')
    >>> instance = ClassWithLogger()
    >>> instance.some_func('this is a warning')
    __main__.ClassWithLogger.some_func: this is a warning
    """
    cls = type(o)
    return logging.getLogger(f'{cls.__module__}.{cls.__name__}')


def scratchFileFactory(analysisName, scratchPath=None, analysisID=0):
    """Create a scratch file path generator.

    Parameters
    ----------
    analysisName : str
        Name of the analysis, e.g. 'SectionAnalysis'.
    scratchPath : path_like
        Path to the scratch directory. If None, uses the system temp directory.
        (default: None)
    analysisID : optional
        Unique ID for the analysis. Useful for parallel execution, for example.
        (default: 0)

    Returns
    -------
    scratchFile
        A function that takes two arguments, 'name' and 'suffix'.

    Example
    -------
    >>> scratchFile = scratchFileFactory('TestoPresto')
    >>> scratchFile('disp', '.dat')
    PosixPath('/tmp/TestoPresto_disp_0.dat')
    """
    if scratchPath is None:
        scratchPath = tempfile.gettempdir()
    scratchPath = pathlib.Path(scratchPath).resolve()

    def scratchFile(name, suffix='') -> pathlib.Path:
        """
        Parameters
        ----------
        name : str
            Name of the scratch file, e.g. 'displacement'.
        suffix : str, optional
            Suffix to use for the scratch file. (default: '')
        """
        return scratchPath / f'{analysisName}_{analysisID}_{name}{suffix}'

    return scratchFile


def fillOutNumbers(peaks, rate, axis=0) -> np.ndarray:
    """Fill in numbers between peaks at a specified rate.

    Parameters
    ----------
    peaks : array_like
        Peaks to fill between.
    rate : float
        Rate to use between peaks.
    axis : int, optional
        Axis to fill along. (default: 0)

    See Also
    --------
    linspacePeaks : Fill in numbers between peaks, using the same number of
                    steps each time (instead of calculating from a given rate).

    Examples
    --------
    >>> fillOutNumbers([0, 1, -1], rate=0.5)
    array([ 0. ,  0.5,  1. ,  0.5,  0. , -0.5, -1. ])
    >>> fillOutNumbers([[0, 1, -1], [1, 2, -2]], rate=0.25)
    array([[ 0.  ,  1.  , -1.  ],
           [ 0.25,  1.25, -1.25],
           [ 0.5 ,  1.5 , -1.5 ],
           [ 0.75,  1.75, -1.75],
           [ 1.  ,  2.  , -2.  ]])

    For multi-dimensional arrays, `rate` is a maximum; the peaks will remain
    lined up, but the actual difference will vary.

    >>> fillOutNumbers([[0, 1, -1], [1, 2, -2]], rate=1.0, axis=1)
    array([[ 0. ,  1. ,  0.5,  0. , -0.5, -1. ],
           [ 1. ,  2. ,  1. ,  0. , -1. , -2. ]])

    When `rate` does not divide the distance between two peaks into an integer
    number of steps, the rounded number of steps is used instead:

    >>> fillOutNumbers([0, 1], rate=0.45)
    array([0.        , 0.33333333, 0.66666667, 1.        ])
    """
    peaks: np.ndarray = np.asanyarray(peaks)
    if peaks.size < 2:
        raise ValueError(f'At least two peaks needed to fill between')

    # Get all axes other than `axis` so we can do reductions down to that axis
    axes = list(range(peaks.ndim))
    axes.pop(axis)
    axes = tuple(axes)

    # Determine the number of steps between each peak.
    peaksDiff = np.diff(peaks, axis=axis)
    numsteps = np.round(np.max(np.abs(peaksDiff/rate), axis=axes)).astype('int')

    numbers = []
    numPeaks = peaks.shape[axis]
    for i in range(numPeaks - 1):
        start = np.take(peaks, i, axis)
        stop = np.take(peaks, i + 1, axis)
        num = numsteps[i]
        numbers.append(np.linspace(start, stop, num, endpoint=False, axis=axis))
    numbers.append(np.take(peaks, [-1], axis))

    return np.concatenate(numbers, axis)
