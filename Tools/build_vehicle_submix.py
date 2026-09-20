# Builds the vehicle output chain: a submix with a limiter on it, and a SoundClass that routes
# every vehicle layer into it.
#
#   layer AudioComponent --(SoundClassOverride from the data asset)--> SCL_VehicleSound
#                                                                       |  default submix
#                                                                       v
#                                                                    SBM_Vehicle --[limiter]--> master
#
# Why this shape:
#
# The data asset's SoundClass was None, so there was nowhere to raise the overall level at all -
# the only volume controls were per-layer multipliers capped at 1. Raising that cap to 4 without
# somewhere for the peaks to go just moves the problem from "too quiet" to "clipping", so the
# limiter is the other half of that change, not a nicety.
#
# The limiter goes on a vehicle submix rather than the master so that crowd and music are not
# squashed every time an engine peaks. It also gives the crowd ducking something to key off later.
#
# Run through the editor Python bridge, or paste into the editor's Python console.
import unreal

PATH = "/Game/VehicleSound"
SUBMIX = "SBM_Vehicle"
CLASS = "SCL_VehicleSound"

tools = unreal.AssetToolsHelpers.get_asset_tools()


def make(name, factory, cls):
    full = "%s/%s" % (PATH, name)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        print("exists, reusing:", full)
        return unreal.EditorAssetLibrary.load_asset(full)
    a = tools.create_asset(name, PATH, cls, factory)
    print("created:", a.get_path_name())
    return a


# ---------------------------------------------------------------- submix + limiter
submix = make(SUBMIX, unreal.SoundSubmixFactory(), unreal.SoundSubmix)

# no factory: this preset has no dedicated one, and the generic submix-effect factory returns
# null here because it expects to ask a human which preset class to make. AssetTools will build
# a plain UObject asset of the class it is handed instead
limiter = make("SFX_VehicleLimiter", None, unreal.SubmixEffectDynamicsProcessorPreset)

s = unreal.SubmixEffectDynamicsProcessorSettings()
s.set_editor_property("dynamics_processor_type", unreal.SubmixEffectDynamicsProcessorType.LIMITER)
# just under 0 dBFS: the point is to catch peaks, not to squash the mix
s.set_editor_property("threshold_db", -1.0)
s.set_editor_property("ratio", 20.0)
# fast enough to catch a crash transient, slow enough not to click on every engine cycle
s.set_editor_property("attack_time_msec", 1.0)
s.set_editor_property("release_time_msec", 100.0)
# a limiter without lookahead is a clipper that arrives late
s.set_editor_property("look_ahead_msec", 3.0)
s.set_editor_property("knee_bandwidth_db", 2.0)
s.set_editor_property("input_gain_db", 0.0)
s.set_editor_property("output_gain_db", 0.0)
limiter.set_editor_property("settings", s)

chain = list(submix.get_editor_property("submix_effect_chain") or [])
if not any(e and e.get_name() == limiter.get_name() for e in chain):
    chain.append(limiter)
    submix.set_editor_property("submix_effect_chain", chain)
    print("limiter added to submix chain")
else:
    print("limiter already in chain")

# ---------------------------------------------------------------- sound class
sound_class = make(CLASS, unreal.SoundClassFactory(), unreal.SoundClass)
props = sound_class.get_editor_property("properties")
props.set_editor_property("default_submix", submix)
# the overall level knob that did not exist before. 1.0 changes nothing on its own; this is the
# dial to turn once the limiter is catching what comes off the top
props.set_editor_property("volume", 1.0)
sound_class.set_editor_property("properties", props)
print("sound class -> submix:", props.get_editor_property("default_submix"))

# ---------------------------------------------------------------- wire the data asset
da = unreal.EditorAssetLibrary.load_asset("%s/DA_SportsCar_Dynamic" % PATH)
before = da.get_editor_property("sound_class")
da.set_editor_property("sound_class", sound_class)
print("DA SoundClass: %s -> %s" % (before, sound_class.get_name()))

for a in (submix, limiter, sound_class, da):
    unreal.EditorAssetLibrary.save_loaded_asset(a)
print("saved")
