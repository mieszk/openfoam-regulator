#include "regulatorLibrary.H"

// * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * * //

scalar Regulator::patchAverage(const word &fieldName, const fvPatch &patch)
{
    const fvPatchField<scalar> &field =
        patch.lookupPatchField<volScalarField, scalar>(fieldName);

    return gSum(field * patch.magSf()) / gSum(patch.magSf());
}

const Foam::Enum<Regulator::operationMode>
    Regulator::operationModeNames({
        {operationMode::twoStep, "twoStep"},
        {operationMode::PID, "PID"},
    });

// * * * * * * * * * * * * * * * * Constructor * * * * * * * * * * * * * * * //

Regulator::Regulator(const fvMesh &mesh, const dictionary &dict)
    : mesh_(mesh),
      regulatedFieldName_(dict.getWord("fieldName")),
      targetPatchName_(dict.getWord("patchName")),
      targetValue_(dict.getScalar("targetValue")),
      mode_(operationModeNames.get("mode", dict)),
      Kp_(0.),
      Ti_(0.),
      Td_(0.),
      outputMax_(1.),
      outputMin_(0.),
      error_(0.),
      errorIntegral_(0.),
      oldError_(0.),
      timeIndex_(mesh.time().timeIndex())
{
    switch (mode_)
    {
        // Two step regulator returns either 0 or 1
        case twoStep:
        {
            outputMax_ = 1.;
            outputMax_ = 0.;
            break;
        }
        // PID returns a value between outputMin and outputMax, defaults to (-1, 1)
        case PID:
        {
            Kp_ = dict.getScalar("Kp");
            Ti_ = dict.getScalar("Ti");
            Td_ = dict.getScalar("Td");
            outputMax_ = dict.getOrDefault<scalar>("outputMax", 1.);
            outputMin_ = dict.getOrDefault<scalar>("outputMin", -1.);
            break;
        }
        default:
        {
            FatalIOError << "Unknown regulator mode: " << mode_ << endl;
            exit(FatalIOError);
        }
    }
}

Regulator::Regulator(const fvMesh &mesh)
    : mesh_(mesh),
    regulatedFieldName_(word::null),
    targetPatchName_(word::null),
    targetValue_(0),
    mode_(PID),
    Kp_(0),
    Ti_(0),
    Td_(0),
    outputMax_(1),
    outputMin_(0),
    error_(0),
    errorIntegral_(0),
    oldError_(0),
    timeIndex_(mesh.time().timeIndex())
{}

Regulator::Regulator(const Regulator &reg)
    : mesh_(reg.mesh_),
      regulatedFieldName_(reg.regulatedFieldName_),
      targetPatchName_(reg.targetPatchName_),
      targetValue_(reg.targetValue_),
      mode_(reg.mode_),
      Kp_(reg.Kp_),
      Ti_(reg.Ti_),
      Td_(reg.Td_),
      outputMax_(reg.outputMax_),
      outputMin_(reg.outputMin_),
      error_(reg.error_),
      errorIntegral_(reg.errorIntegral_),
      oldError_(reg.oldError_),
      timeIndex_(reg.timeIndex_)
{}

// * * * * * * * * * * * * Public Member Functions  * * * * * * * * * * * * *//

scalar Regulator::probeTargetPatch() const
{
    // Get the target patch average field value
    const fvPatch &targetPatch = mesh_.boundary()[targetPatchName_];
    const scalar result = patchAverage(regulatedFieldName_, targetPatch);
    return result;
}

scalar Regulator::read()
{
    // Get the time step
    const scalar deltaT(mesh_.time().deltaTValue());

    // Update the old-time quantities
    if (timeIndex_ != mesh_.time().timeIndex())
    {
        timeIndex_ = mesh_.time().timeIndex();
        oldError_ = error_;
    }

    // Get the target patch average field value
    const scalar currentRegulatedPatchValue = probeTargetPatch();

    // Calculate errors
    scalar result = 0.;
    error_ = targetValue_ - currentRegulatedPatchValue;

    switch (mode_)
    {
        case twoStep:
        {
            result = error_ <= 0 ? 0 : 1;
            break;
        }
        case PID:
        {
            errorIntegral_ += 0.5*(error_ + oldError_)*deltaT;
            const scalar errorDifferential = (error_ - oldError_) / deltaT;

            // Calculate output signal
            // A negliable value is added to Ti_ to prevent division by 0
            const scalar outputSignal = Kp_*(error_ + 1/(Ti_ + 1e-7)*errorIntegral_ + Td_*errorDifferential);

            // Return result within defined regulator saturation: outputMax_ and outputMin_
            result = max(min(outputSignal, outputMax_), outputMin_);
            break;
        }
    }

    Info << "Time index: " << mesh_.time().timeIndex() << endl;
    Info << "regulatedFieldName: " << regulatedFieldName_ << endl;
    Info << "targetPatchName: " << targetPatchName_ << endl;
    Info << "targetValue: " << targetValue_ << endl;
    Info << "currentRegulatedPatchValue: " << currentRegulatedPatchValue << endl;
    Info << "Error: " << error_ << endl;
    Info << "Output signal: " << result << endl;

    return result;
}
