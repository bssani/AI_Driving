# Builds /Game/VehicleSound/Crowd/MS_Crowd - the bed a grandstand plays.
#
# A crowd is not one recording at two volumes. A murmur and a roar are different sounds: the
# murmur is broadband and even, the roar has shape and peaks. Crossfading between two recordings
# on Excitement is the whole point of this graph, and it is why the stand sends a parameter rather
# than simply turning a loop up.
#
# NO DOPPLER. There is a Delay Pitch Shift node and it must not go in here. UE5 computes doppler
# from relative velocity, so a car passing at racing speed would sweep the pitch of the entire
# stand. People do not change pitch as you drive past them, and on a bed this wide it reads as
# nausea rather than speed. (Doppler is not an attenuation setting in UE5 - it only exists as
# that node - so leaving it out of the graph is all that is needed.)
#
# Run through the editor Python bridge, or paste into the editor console.
import unreal

ASSET_PATH = "/Game/VehicleSound/Crowd"
ASSET_NAME = "MS_Crowd"

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


# stereo: a crowd has width, and unlike an engine it is not a point being localised. The box
# attenuation places it; the stereo field gives it body once you are close.
builder, on_play, on_finished_in, audio_outs, res = bs.create_source_builder(
    ASSET_NAME, unreal.MetaSoundOutputAudioFormat.STEREO, False)
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
IN_EXCITE = gin("Excitement", "Float", F(0.0))

IN_W_MURMUR = gin("Wave Murmur", "WaveAsset", OBJ(None))
IN_W_ROAR = gin("Wave Roar", "WaveAsset", OBJ(None))

IN_LVL_MURMUR = gin("Murmur Level", "Float", F(1.0))
IN_LVL_ROAR = gin("Roar Level", "Float", F(1.0))

# a crowd on its feet is brighter as well as louder: more voices in the top end, fewer bodies
# absorbing it. Moving only the level makes the roar sound like the murmur turned up
IN_CUT_CALM = gin("Calm Cutoff", "Float", F(4500.0))
IN_CUT_LOUD = gin("Excited Cutoff", "Float", F(14000.0))

# ---------------------------------------------------------------- the two beds
for tag, wave_in, lvl_in in (("murmur", IN_W_MURMUR, IN_LVL_MURMUR),
                             ("roar", IN_W_ROAR, IN_LVL_ROAR)):
    add("wave_" + tag, "UE", "Wave Player", "Stereo")
    wire_in(wave_in, "wave_" + tag, "Wave Asset", "wave " + tag)
    ok(builder.set_node_input_default(nin("wave_" + tag, "Loop"), B(True)), "loop " + tag)
    ok(builder.connect_nodes(on_play, nin("wave_" + tag, "Play")), "on play -> wave_" + tag)

    for side in ("Left", "Right"):
        add("gain_%s_%s" % (tag, side), "UE", "Multiply", "Audio by Float")
        wire("wave_" + tag, "Out " + side, "gain_%s_%s" % (tag, side), "PrimaryOperand")
        wire_in(lvl_in, "gain_%s_%s" % (tag, side), "AdditionalOperands", "%s level" % tag)

# ---------------------------------------------------------------- excitement
# smoothed here as well as in the subsystem. The subsystem interpolates at its own update rate,
# which is ten times a second; this takes the remaining steps off so the crossfade does not
# zipper on a sound with no transients to hide it
add("smooth", "UE", "InterpTo", "Audio")
ok(builder.set_node_input_default(nin("smooth", "Interp Time"), F(0.35)), "excitement smoothing")
wire_in(IN_EXCITE, "smooth", "Target", "Excitement")

for side in ("Left", "Right"):
    add("blend_" + side, "Crossfade", "Trigger Route (Audio, 2)", "Audio")
    wire("smooth", "Value", "blend_" + side, "Crossfade Value")
    wire("gain_murmur_" + side, "Out", "blend_" + side, "In 0")
    wire("gain_roar_" + side, "Out", "blend_" + side, "In 1")

# ---------------------------------------------------------------- brightness
add("cutoff", "MapRange", "MapRange", "Float")
wire("smooth", "Value", "cutoff", "In")
ok(builder.set_node_input_default(nin("cutoff", "In Range A"), F(0.0)), "cutoff in A")
ok(builder.set_node_input_default(nin("cutoff", "In Range B"), F(1.0)), "cutoff in B")
wire_in(IN_CUT_CALM, "cutoff", "Out Range A", "calm cutoff")
wire_in(IN_CUT_LOUD, "cutoff", "Out Range B", "excited cutoff")
ok(builder.set_node_input_default(nin("cutoff", "Clamped"), B(True)), "cutoff clamped")

for side in ("Left", "Right"):
    add("lpf_" + side, "UE", "One-Pole Low Pass Filter", "Audio")
    wire("blend_" + side, "Out", "lpf_" + side, "In")
    wire("cutoff", "Out Value", "lpf_" + side, "Cutoff Frequency")

# ---------------------------------------------------------------- out
if len(audio_outs) < 2:
    raise RuntimeError("expected a stereo output pair, got %d" % len(audio_outs))
ok(builder.connect_nodes(nout("lpf_Left", "Out"), audio_outs[0]), "left -> out")
ok(builder.connect_nodes(nout("lpf_Right", "Out"), audio_outs[1]), "right -> out")

print("built %d nodes" % len(NODES))

full = ASSET_PATH + "/" + ASSET_NAME
if unreal.EditorAssetLibrary.does_asset_exist(full):
    print("replacing existing", full)
    unreal.EditorAssetLibrary.delete_asset(full)

asset, res = es.build_to_asset(builder, "MetaSoundSource", ASSET_NAME, ASSET_PATH)
ok(res, "build_to_asset")

unreal.EditorAssetLibrary.save_asset(full)

# route the bed into the crowd submix, which is where the ducking lives.
# build_to_asset hands back the document interface rather than the source, and that does not
# carry the sound properties - load the asset itself
source = unreal.EditorAssetLibrary.load_asset(full)
submix = unreal.EditorAssetLibrary.load_asset("/Game/VehicleSound/Crowd/SBM_Crowd")
if source and submix:
    source.set_editor_property("sound_submix_object", submix)
    unreal.EditorAssetLibrary.save_loaded_asset(source)
    print("routed to SBM_Crowd")

print("SAVED:", full)
