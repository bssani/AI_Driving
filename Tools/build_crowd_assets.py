# Builds the crowd audio chain: a box attenuation for a grandstand, a crowd submix ducked by the
# vehicle submix, and the MetaSound bed the stands play.
#
#   ARaceCrowdStand -> MS_Crowd --(Att_CrowdStand, box)--> SBM_Crowd --[ducker keyed off
#                                                           SBM_Vehicle]--> master
#
# Run through the editor Python bridge, or paste into the editor console.
import unreal

PATH = "/Game/VehicleSound/Crowd"
tools = unreal.AssetToolsHelpers.get_asset_tools()


def make(name, factory, cls):
    full = "%s/%s" % (PATH, name)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        print("exists, reusing:", full)
        return unreal.EditorAssetLibrary.load_asset(full)
    a = tools.create_asset(name, PATH, cls, factory)
    if a is None:
        raise RuntimeError("could not create " + full)
    print("created:", a.get_path_name())
    return a


# ---------------------------------------------------------------- attenuation
# A grandstand is a wall of people, not a speaker. A sphere behaves like its centre as soon as the
# listener is outside it, so driving past makes the whole crowd swing around the head - the one
# thing a real crowd never does. A box holds still and has width.
att = make("Att_CrowdStand", unreal.SoundAttenuationFactory(), unreal.SoundAttenuation)
s = att.get_editor_property("attenuation")
s.set_editor_property("attenuation_shape", unreal.AttenuationShape.BOX)
# half-extents: 30 m of stand, 8 m deep, 6 m tall. Split a longer stand across several actors
# rather than stretching one box, or the middle of it has no detail
s.set_editor_property("attenuation_shape_extents", unreal.Vector(3000.0, 800.0, 600.0))
s.set_editor_property("falloff_distance", 6000.0)
s.set_editor_property("attenuate", True)
s.set_editor_property("spatialize", True)
# people do not get quieter in a straight line with distance
s.set_editor_property("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND)
s.set_editor_property("d_b_attenuation_at_max", -36.0)
# air takes the top off a distant crowd; without this a far stand sounds like a near one turned down
s.set_editor_property("attenuate_with_lpf", True)
s.set_editor_property("lpf_frequency_at_min", 20000.0)
s.set_editor_property("lpf_frequency_at_max", 3500.0)
s.set_editor_property("enable_reverb_send", True)
att.set_editor_property("attenuation", s)

# ---------------------------------------------------------------- submix + ducker
crowd_submix = make("SBM_Crowd", unreal.SoundSubmixFactory(), unreal.SoundSubmix)

vehicle_submix = unreal.EditorAssetLibrary.load_asset("/Game/VehicleSound/SBM_Vehicle")
if vehicle_submix is None:
    raise RuntimeError("SBM_Vehicle missing - run the vehicle submix builder first")

# no factory: this preset has no dedicated one and the generic submix-effect factory expects to
# ask a human which class to make
ducker = make("SFX_CrowdDucker", None, unreal.SubmixEffectDynamicsProcessorPreset)

d = unreal.SubmixEffectDynamicsProcessorSettings()
d.set_editor_property("dynamics_processor_type", unreal.SubmixEffectDynamicsProcessorType.COMPRESSOR)
# keyed off the cars, not off itself. Engines and a crowd occupy the same part of the spectrum and
# the engine has to win - a crowd that stays at full level while a car goes past does not sound
# loud, it sounds like it is in front of the car instead of behind it
d.set_editor_property("key_source", unreal.SubmixEffectDynamicsKeySource.SUBMIX)
d.set_editor_property("external_submix", vehicle_submix)
d.set_editor_property("threshold_db", -22.0)
d.set_editor_property("ratio", 3.0)
# slow enough that it breathes rather than pumps on every engine cycle
d.set_editor_property("attack_time_msec", 40.0)
d.set_editor_property("release_time_msec", 400.0)
d.set_editor_property("knee_bandwidth_db", 12.0)
d.set_editor_property("peak_mode", unreal.SubmixEffectDynamicsPeakMode.ROOT_MEAN_SQUARED)
ducker.set_editor_property("settings", d)

chain = list(crowd_submix.get_editor_property("submix_effect_chain") or [])
if not any(e and e.get_name() == ducker.get_name() for e in chain):
    chain.append(ducker)
    crowd_submix.set_editor_property("submix_effect_chain", chain)
    print("ducker added to crowd submix chain")

for a in (att, crowd_submix, ducker):
    unreal.EditorAssetLibrary.save_loaded_asset(a)
print("saved crowd routing")
