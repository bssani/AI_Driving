# Builds the exhaust MetaSound as code, so the graph is reproducible and reviewable.
#
# Run through the editor Python bridge (UnrealAgent, 127.0.0.1:55559 POST /mcp,
# tools/call execute_python), or paste into the editor's Python console.
#
# Builds /Game/VehicleSound/MS_Exhaust.
#
# Structure only. Every Wave Asset is left empty and exposed as a graph input, so the samples and
# the tuning are set in the MetaSound's Inputs panel without opening the graph.
#
# Chain:
#   three RPM-layered loops, each pitched by RPM / (the RPM it was recorded at), blended by
#   NormalizedRPM -> the on-throttle bus
#   a fourth loop for overrun, pitched the same way -> the off-throttle bus
#   the two crossfaded by ExhaustValveOpen (smoothed) -> low-pass whose cutoff follows InCarAmount
import unreal

ASSET_PATH = "/Game/VehicleSound"
ASSET_NAME = "MS_Exhaust"

bs = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
es = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)


def CN(ns, n, v=""):
    c = unreal.MetasoundFrontendClassName()
    c.namespace = ns
    c.name = n
    c.variant = v
    return c


def ok(res, what):
    if "SUCCEEDED" not in str(res).upper():
        raise RuntimeError("%s -> %s" % (what, res))


# not one-shot: a continuous layer wants a source that runs until it is stopped. The pack's engine
# graphs carry the one-shot interface and end on their own, which is why the plugin has to watch
# for a layer that went silent and restart it.
builder, on_play, on_finished_in, audio_outs, res = bs.create_source_builder(
    ASSET_NAME, unreal.MetaSoundOutputAudioFormat.MONO, False)
ok(res, "create_source_builder")

NODES = {}


def add(key, ns, n, v=""):
    h, r = builder.add_node_by_class_name(CN(ns, n, v), 1)
    ok(r, "add %s (%s|%s|%s)" % (key, ns, n, v))
    NODES[key] = h
    return h


def nin(key, pin):
    h, r = builder.find_node_input_by_name(NODES[key], pin)
    ok(r, "input %s.%s" % (key, pin))
    return h


def nout(key, pin):
    h, r = builder.find_node_output_by_name(NODES[key], pin)
    ok(r, "output %s.%s" % (key, pin))
    return h


def wire(from_key, from_pin, to_key, to_pin):
    ok(builder.connect_nodes(nout(from_key, from_pin), nin(to_key, to_pin)),
       "%s.%s -> %s.%s" % (from_key, from_pin, to_key, to_pin))


def gin(name, data_type, literal, constructor=False):
    h, r = builder.add_graph_input_node(name, data_type, literal, constructor)
    ok(r, "graph input " + name)
    return h


def wire_in(input_handle, to_key, to_pin, label):
    ok(builder.connect_nodes(input_handle, nin(to_key, to_pin)),
       "input %s -> %s.%s" % (label, to_key, to_pin))


def lit(maker, value):
    # these return (literal, result); the builder wants only the literal
    r = maker(value)
    return r[0] if isinstance(r, tuple) else r


def F(v):
    return lit(bs.create_float_meta_sound_literal, v)


def B(v):
    return lit(bs.create_bool_meta_sound_literal, v)


def OBJ(v):
    return lit(bs.create_object_meta_sound_literal, v)

# ---------------------------------------------------------------- graph inputs
# what the ExhaustSoundLayer sends
IN_RPM = gin("RPM", "Float", F(800.0))
IN_NORM_RPM = gin("NormalizedRPM", "Float", F(0.0))
IN_VALVE = gin("ExhaustValveOpen", "Float", F(0.0))
IN_INCAR = gin("InCarAmount", "Float", F(0.0))

# the samples. Left empty on purpose - these are the four slots to fill
IN_W_LOW = gin("Wave Low", "WaveAsset", OBJ(None))
IN_W_MID = gin("Wave Mid", "WaveAsset", OBJ(None))
IN_W_HIGH = gin("Wave High", "WaveAsset", OBJ(None))
IN_W_OVER = gin("Wave Overrun", "WaveAsset", OBJ(None))

# the RPM each sample was recorded at. Pitch is RPM / this, so a wrong number here is the single
# most audible mistake available: the loop then plays at the wrong speed everywhere
IN_RPM_LOW = gin("Low Sample RPM", "Float", F(1200.0))
IN_RPM_MID = gin("Mid Sample RPM", "Float", F(3500.0))
IN_RPM_HIGH = gin("High Sample RPM", "Float", F(6000.0))
IN_RPM_OVER = gin("Overrun Sample RPM", "Float", F(2500.0))

# ear-tuning knobs
IN_LVL_ON = gin("On Throttle Level", "Float", F(1.0))
IN_LVL_OVER = gin("Overrun Level", "Float", F(1.0))
IN_CUT_OUT = gin("Outside Cutoff", "Float", F(12000.0))
IN_CUT_IN = gin("In Car Cutoff", "Float", F(1400.0))

# ---------------------------------------------------------------- pitch per layer
# semitones = the frequency multiplier expressed in semitones, and the multiplier is simply how
# far the engine is from where the sample was recorded. The ratio is clamped to two octaves either
# way: past that a resampled loop stops sounding like an engine and starts sounding like a fault,
# and it also keeps a sample RPM left at zero from producing an infinity
for tag, wave_in, rpm_in in (("low", IN_W_LOW, IN_RPM_LOW),
                             ("mid", IN_W_MID, IN_RPM_MID),
                             ("high", IN_W_HIGH, IN_RPM_HIGH),
                             ("over", IN_W_OVER, IN_RPM_OVER)):
    add("div_" + tag, "UE", "Divide", "Float")
    add("clmp_" + tag, "Clamp", "Clamp", "Float")
    add("semi_" + tag, "UE", "Frequency Multiplier to Semitone", "Float")
    add("wave_" + tag, "UE", "Wave Player", "Mono")

    wire_in(IN_RPM, "div_" + tag, "PrimaryOperand", "RPM")
    wire_in(rpm_in, "div_" + tag, "AdditionalOperands", "sample RPM")
    wire("div_" + tag, "Out", "clmp_" + tag, "In")
    ok(builder.set_node_input_default(nin("clmp_" + tag, "Min"), F(0.25)), "clamp min " + tag)
    ok(builder.set_node_input_default(nin("clmp_" + tag, "Max"), F(4.0)), "clamp max " + tag)
    wire("clmp_" + tag, "Value", "semi_" + tag, "Frequency Multiplier")
    wire("semi_" + tag, "Semitones", "wave_" + tag, "Pitch Shift")

    wire_in(wave_in, "wave_" + tag, "Wave Asset", "wave " + tag)
    ok(builder.set_node_input_default(nin("wave_" + tag, "Loop"), B(True)), "loop " + tag)
    # start every loop when the source starts; nothing else ever triggers them
    ok(builder.connect_nodes(on_play, nin("wave_" + tag, "Play")), "on play -> wave_" + tag)

# ---------------------------------------------------------------- RPM blend
# smoothed, because NormalizedRPM arrives once per game frame and a crossfade stepping at frame
# rate is audible as zipper noise across the blend
add("smooth_rpm", "UE", "InterpTo", "Audio")
ok(builder.set_node_input_default(nin("smooth_rpm", "Interp Time"), F(0.08)), "rpm smoothing time")
wire_in(IN_NORM_RPM, "smooth_rpm", "Target", "NormalizedRPM")

add("blend_rpm", "Crossfade", "Trigger Route (Audio, 3)", "Audio")
wire("smooth_rpm", "Value", "blend_rpm", "Crossfade Value")
wire("wave_low", "Out Mono", "blend_rpm", "In 0")
wire("wave_mid", "Out Mono", "blend_rpm", "In 1")
wire("wave_high", "Out Mono", "blend_rpm", "In 2")

# ---------------------------------------------------------------- levels
add("gain_on", "UE", "Multiply", "Audio by Float")
wire("blend_rpm", "Out", "gain_on", "PrimaryOperand")
wire_in(IN_LVL_ON, "gain_on", "AdditionalOperands", "on throttle level")

add("gain_over", "UE", "Multiply", "Audio by Float")
wire("wave_over", "Out Mono", "gain_over", "PrimaryOperand")
wire_in(IN_LVL_OVER, "gain_over", "AdditionalOperands", "overrun level")

# ---------------------------------------------------------------- throttle blend
# ExhaustValveOpen rather than the OnThrottle bool: this crossfades rather than switches, and a
# blend wants the amount. A hard 0/1 here would make feathering the throttle sound like a relay
add("smooth_valve", "UE", "InterpTo", "Audio")
ok(builder.set_node_input_default(nin("smooth_valve", "Interp Time"), F(0.05)), "valve smoothing time")
wire_in(IN_VALVE, "smooth_valve", "Target", "ExhaustValveOpen")

add("blend_thr", "Crossfade", "Trigger Route (Audio, 2)", "Audio")
wire("smooth_valve", "Value", "blend_thr", "Crossfade Value")
wire("gain_over", "Out", "blend_thr", "In 0")   # valve shut -> overrun
wire("gain_on", "Out", "blend_thr", "In 1")     # valve open -> on throttle

# ---------------------------------------------------------------- cabin filter
# an exhaust heard through a bulkhead is not the same sound as one heard from the pavement, and
# in VR the driver is always on the wrong side of that bulkhead
add("cutoff", "MapRange", "MapRange", "Float")
wire_in(IN_INCAR, "cutoff", "In", "InCarAmount")
ok(builder.set_node_input_default(nin("cutoff", "In Range A"), F(0.0)), "cutoff in A")
ok(builder.set_node_input_default(nin("cutoff", "In Range B"), F(1.0)), "cutoff in B")
wire_in(IN_CUT_OUT, "cutoff", "Out Range A", "outside cutoff")
wire_in(IN_CUT_IN, "cutoff", "Out Range B", "in car cutoff")
ok(builder.set_node_input_default(nin("cutoff", "Clamped"), B(True)), "cutoff clamped")

add("lpf", "UE", "One-Pole Low Pass Filter", "Audio")
wire("blend_thr", "Out", "lpf", "In")
wire("cutoff", "Out Value", "lpf", "Cutoff Frequency")

# ---------------------------------------------------------------- out
for out_in in audio_outs:
    ok(builder.connect_nodes(nout("lpf", "Out"), out_in), "lpf -> audio out")

print("built %d nodes" % len(NODES))

# Re-runnable: the old asset is removed first so a second run replaces the graph rather than
# landing beside it as MS_Exhaust_1. Anything set in the Inputs panel is lost on a rebuild, so
# write down the sample assignments before re-running this.
full = ASSET_PATH + "/" + ASSET_NAME
if unreal.EditorAssetLibrary.does_asset_exist(full):
    print("replacing existing", full)
    unreal.EditorAssetLibrary.delete_asset(full)

asset, res = es.build_to_asset(builder, "MetaSoundSource", ASSET_NAME, ASSET_PATH)
ok(res, "build_to_asset")
unreal.EditorAssetLibrary.save_asset(ASSET_PATH + "/" + ASSET_NAME)
print("SAVED:", asset.get_path_name())
