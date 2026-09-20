# Builds the engine MetaSound as code, so the graph is reproducible and reviewable.
#
# Run through the editor Python bridge (UnrealAgent, 127.0.0.1:55559 POST /mcp,
# tools/call execute_python), or paste into the editor's Python console.
#
# Builds /Game/VehicleSound/MS_Engine.
#
# This is an ALTERNATIVE to the pack's MS_Sportscar / MS_Supercar, not a replacement. Assign it in
# the data asset to A/B them by ear. What it does that they do not:
#
#   - uses the on/off throttle distinction, which is the single biggest thing missing from the
#     pack graphs: they take RPM and nothing about load, so 4000 rpm climbing under power and
#     4000 rpm falling on a shut throttle come out identical
#   - runs until stopped rather than carrying the one-shot interface, which is why the plugin
#     has to watch for a layer that ended on its own and restart it
#   - knows whether the listener is in the cabin
#
# The pack has no off-throttle recordings, so overrun is faked the way it is normally faked when
# you only have on-throttle loops: off the throttle the engine gets quieter and duller. It is not
# a substitute for a real trailing-throttle set, but it is the difference between a car that
# breathes with the driver and a siren.
#
# Chain:
#   three RPM-layered loops, each pitched by RPM / (the RPM it was recorded at), blended by
#   NormalizedRPM, then level and brightness both pulled down off the throttle, then the cabin
#   filter on top.
import unreal

ASSET_PATH = "/Game/VehicleSound"
ASSET_NAME = "MS_Engine"

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


def wire(fk, fp, tk, tp):
    ok(builder.connect_nodes(nout(fk, fp), nin(tk, tp)), "%s.%s -> %s.%s" % (fk, fp, tk, tp))


def gin(name, data_type, literal, constructor=False):
    h, r = builder.add_graph_input_node(name, data_type, literal, constructor)
    ok(r, "graph input " + name)
    return h


def wire_in(handle, tk, tp, label):
    ok(builder.connect_nodes(handle, nin(tk, tp)), "input %s -> %s.%s" % (label, tk, tp))


def lit(maker, value):
    r = maker(value)
    return r[0] if isinstance(r, tuple) else r


def F(v):
    return lit(bs.create_float_meta_sound_literal, v)


def B(v):
    return lit(bs.create_bool_meta_sound_literal, v)


def OBJ(v):
    return lit(bs.create_object_meta_sound_literal, v)


# ---------------------------------------------------------------- graph inputs
IN_RPM = gin("RPM", "Float", F(900.0))
IN_NORM_RPM = gin("NormalizedRPM", "Float", F(0.0))
IN_THROTTLE = gin("OnThrottleAmount", "Float", F(0.0))
IN_INCAR = gin("InCarAmount", "Float", F(0.0))

# For the SportsCar the pack already has a complete set: SportLowEngine, SportNormalEngine,
# SportFastlEngine. The middle one is currently used by nothing.
IN_W_LOW = gin("Wave Low", "WaveAsset", OBJ(None))
IN_W_MID = gin("Wave Mid", "WaveAsset", OBJ(None))
IN_W_HIGH = gin("Wave High", "WaveAsset", OBJ(None))

# the RPM each sample was recorded at. Pitch is RPM / this, so a wrong number here is the single
# most audible mistake available: the loop then plays at the wrong speed everywhere
IN_RPM_LOW = gin("Low Sample RPM", "Float", F(900.0))
IN_RPM_MID = gin("Mid Sample RPM", "Float", F(3500.0))
IN_RPM_HIGH = gin("High Sample RPM", "Float", F(6000.0))

# ear-tuning knobs
IN_LVL_ON = gin("On Throttle Level", "Float", F(1.0))
IN_LVL_OFF = gin("Off Throttle Level", "Float", F(0.55))
IN_CUT_ON = gin("On Throttle Cutoff", "Float", F(14000.0))
IN_CUT_OFF = gin("Off Throttle Cutoff", "Float", F(3000.0))
IN_CUT_OUT = gin("Outside Cutoff", "Float", F(20000.0))
IN_CUT_IN = gin("In Car Cutoff", "Float", F(5000.0))

# ---------------------------------------------------------------- pitch per layer
# semitones = the frequency multiplier expressed in semitones, and the multiplier is simply how
# far the engine is from where the sample was recorded. The ratio is clamped to two octaves
# either way: past that a resampled loop stops sounding like an engine and starts sounding like
# a fault, and it also keeps a sample RPM left at zero from producing an infinity
for tag, wave_in, rpm_in in (("low", IN_W_LOW, IN_RPM_LOW),
                             ("mid", IN_W_MID, IN_RPM_MID),
                             ("high", IN_W_HIGH, IN_RPM_HIGH)):
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
    ok(builder.connect_nodes(on_play, nin("wave_" + tag, "Play")), "on play -> wave_" + tag)

# ---------------------------------------------------------------- RPM blend
# smoothed, because NormalizedRPM arrives once per game frame and a crossfade stepping at frame
# rate is audible as zipper noise across the blend
add("smooth_rpm", "UE", "InterpTo", "Audio")
ok(builder.set_node_input_default(nin("smooth_rpm", "Interp Time"), F(0.08)), "rpm smoothing")
wire_in(IN_NORM_RPM, "smooth_rpm", "Target", "NormalizedRPM")

add("blend_rpm", "Crossfade", "Trigger Route (Audio, 3)", "Audio")
wire("smooth_rpm", "Value", "blend_rpm", "Crossfade Value")
wire("wave_low", "Out Mono", "blend_rpm", "In 0")
wire("wave_mid", "Out Mono", "blend_rpm", "In 1")
wire("wave_high", "Out Mono", "blend_rpm", "In 2")

# ---------------------------------------------------------------- throttle
# OnThrottleAmount arrives as a hard 0 or 1 - the hysteresis that decides it lives in C++, where
# it belongs, because it is a decision about the driver rather than about audio. Smoothing it
# here is what turns that decision into a transition instead of a switch click.
add("smooth_thr", "UE", "InterpTo", "Audio")
ok(builder.set_node_input_default(nin("smooth_thr", "Interp Time"), F(0.12)), "throttle smoothing")
wire_in(IN_THROTTLE, "smooth_thr", "Target", "OnThrottleAmount")

# off the throttle an engine is quieter AND duller. Doing only the level makes it sound distant;
# doing only the filter makes it sound blanketed. Both together is what reads as backing off.
add("lvl", "MapRange", "MapRange", "Float")
wire("smooth_thr", "Value", "lvl", "In")
ok(builder.set_node_input_default(nin("lvl", "In Range A"), F(0.0)), "lvl in A")
ok(builder.set_node_input_default(nin("lvl", "In Range B"), F(1.0)), "lvl in B")
wire_in(IN_LVL_OFF, "lvl", "Out Range A", "off throttle level")
wire_in(IN_LVL_ON, "lvl", "Out Range B", "on throttle level")
ok(builder.set_node_input_default(nin("lvl", "Clamped"), B(True)), "lvl clamped")

add("cut_thr", "MapRange", "MapRange", "Float")
wire("smooth_thr", "Value", "cut_thr", "In")
ok(builder.set_node_input_default(nin("cut_thr", "In Range A"), F(0.0)), "cut_thr in A")
ok(builder.set_node_input_default(nin("cut_thr", "In Range B"), F(1.0)), "cut_thr in B")
wire_in(IN_CUT_OFF, "cut_thr", "Out Range A", "off throttle cutoff")
wire_in(IN_CUT_ON, "cut_thr", "Out Range B", "on throttle cutoff")
ok(builder.set_node_input_default(nin("cut_thr", "Clamped"), B(True)), "cut_thr clamped")

add("gain", "UE", "Multiply", "Audio by Float")
wire("blend_rpm", "Out", "gain", "PrimaryOperand")
wire("lvl", "Out Value", "gain", "AdditionalOperands")

# ---------------------------------------------------------------- cabin filter
add("cut_car", "MapRange", "MapRange", "Float")
wire_in(IN_INCAR, "cut_car", "In", "InCarAmount")
ok(builder.set_node_input_default(nin("cut_car", "In Range A"), F(0.0)), "cut_car in A")
ok(builder.set_node_input_default(nin("cut_car", "In Range B"), F(1.0)), "cut_car in B")
wire_in(IN_CUT_OUT, "cut_car", "Out Range A", "outside cutoff")
wire_in(IN_CUT_IN, "cut_car", "Out Range B", "in car cutoff")
ok(builder.set_node_input_default(nin("cut_car", "Clamped"), B(True)), "cut_car clamped")

# both the shut throttle and the bulkhead take the top off the sound, and whichever takes more
# wins. Multiplying or averaging them would let an open throttle undo the cabin, which is not how
# a wall works.
add("cut_min", "Min", "Min", "Float")
wire("cut_thr", "Out Value", "cut_min", "A")
wire("cut_car", "Out Value", "cut_min", "B")

add("lpf", "UE", "One-Pole Low Pass Filter", "Audio")
wire("gain", "Out", "lpf", "In")
wire("cut_min", "Value", "lpf", "Cutoff Frequency")

# ---------------------------------------------------------------- out
for out_in in audio_outs:
    ok(builder.connect_nodes(nout("lpf", "Out"), out_in), "lpf -> audio out")

print("built %d nodes" % len(NODES))

# Re-runnable: the old asset is removed first so a second run replaces the graph rather than
# landing beside it as MS_Engine_1. Anything set in the Inputs panel is lost on a rebuild, so
# write down the sample assignments before re-running this.
full = ASSET_PATH + "/" + ASSET_NAME
if unreal.EditorAssetLibrary.does_asset_exist(full):
    print("replacing existing", full)
    unreal.EditorAssetLibrary.delete_asset(full)

asset, res = es.build_to_asset(builder, "MetaSoundSource", ASSET_NAME, ASSET_PATH)
ok(res, "build_to_asset")
unreal.EditorAssetLibrary.save_asset(full)
print("SAVED:", asset.get_path_name())
