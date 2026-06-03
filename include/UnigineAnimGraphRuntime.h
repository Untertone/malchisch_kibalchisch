/* Copyright (C) 2005-2026, UNIGINE. All rights reserved.
*
* This file is a part of the UNIGINE 2 SDK.
*
* Your use and / or redistribution of this software in source and / or
* binary form, with or without modification, is subject to: (i) your
* ongoing acceptance of and compliance with the terms and conditions of
* the UNIGINE License Agreement; and (ii) your inclusion of this notice
* in any version of this software that you use or redistribute.
* A copy of the UNIGINE License Agreement is available by contacting
* UNIGINE. at http://unigine.com/
*/

#pragma once
#include <UnigineMathLib.h>
#include <UnigineNodes.h>
#include <UnigineString.h>

namespace Unigine
{

namespace AnimGraphRuntime
{

using namespace Math;

struct AnimGraphContext;
struct AnimNodeBase;
struct AnimNodeAnimationPlayer;
struct AnimNodeBlendSpace2DSync;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Sync Member (tagged union for unified syncker)
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct SyncMember
{
	enum class KIND { PLAYER = 0, BS2D_SYNC };
	enum class ROLE { LEADER = 0, WEIGHTED_LEADER_AND_FOLLOWER, FOLLOWER };

	KIND kind{KIND::PLAYER};
	AnimNodeBase *node{nullptr};

	// Uniform accessors - bodies defined after complete types
	float getDuration() const;
	float getCurrentTime() const;
	float getAccumulatedWeight() const;
	bool wasInactive() const;
	ROLE getRole() const;
	bool needsUpdate() const;
	bool isLoop() const;
	float getLeaderNextTime(float ifps) const;
	bool isTimeForward() const;

	void applyLeaderTime(float next_time, bool is_forward);
	void applyFollowerTime(float normalized_time, bool is_forward);
	void intersectMarkers(AnimGraphContext &ctx, Vector<unsigned int> &hashes, bool &first) const;
	bool getMarkerSyncInterval(AnimGraphContext &ctx, float time, unsigned int &ph, unsigned int &nh, float &r, const Vector<unsigned int> &h) const;
	void applyMarkerTime(AnimGraphContext &ctx, unsigned int ph, unsigned int nh, float r, const Vector<unsigned int> &h, bool is_forward, float fallback_nt);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Syncker (unified synchronization for Player and BS2DSync)
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimSyncker
{
	using ROLE = SyncMember::ROLE;

	enum class SYNC_METHOD
	{
		NORMALIZED_TIME = 0,
		MARKER_SYNC,
	};

	Vector<SyncMember> members;
	float normalized_time{0.0f};
	bool is_valid_leader{true};
	int leader_index{-1};

	SYNC_METHOD sync_method{SYNC_METHOD::NORMALIZED_TIME};
	Vector<unsigned int> common_hashes;

	bool hasOtherActiveMembers(const AnimNodeBase *exclude) const; // defined after BS2DSync
	void rebuildCommonHashes(AnimGraphContext &context, bool before_update = false); // defined after BS2DSync
	bool syncTimeByMarkers(AnimGraphContext &context, AnimNodeBase *target_node, bool before_update = false); // defined after BS2DSync
	void update(AnimGraphContext &context); // defined after BS2DSync
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Graph Context
///////////////////////////////////////////////////////////////////////////////////////////////////////

struct AnimGraphContext
{
	NodeSkeletonPosePtr skeleton_pose;
	float ifps{0.0f};

	Vector<AnimNodeAnimationPlayer *> self_timed_players;

	bool is_root_motion_enabled{false};
	Transform root_motion_delta;

	int tmp_layers_count{0};

	HashMap<String, int> syncker_name_to_index;
	Vector<AnimSyncker> synckers;

	// state machine context
	struct SMContext
	{
		float state_time{0.0f};
		bool anim_ended{false};
		float anim_time_remaining{0.0f};
		float anim_time_remaining_fraction{0.0f};
		float anim_length{0.0f};
		float anim_time{0.0f};
		float anim_time_fraction{0.0f};
	};
	SMContext sm;
	Vector<SMContext> sm_stack;

	void pushSMContext() { sm_stack.append(sm); sm = {}; }
	void popSMContext()  { sm = sm_stack.last(); sm_stack.removeLast(); }

	bool isActiveRootMotion() const { return is_root_motion_enabled; }

	int getTmpPoseLayer()
	{
		tmp_layers_count++;
		return skeleton_pose->addLayer();
	}

	void releaseTmpPoseLayers()
	{
		skeleton_pose->setNumLayers(skeleton_pose->getNumLayers() - tmp_layers_count);
		tmp_layers_count = 0;
	}

	int addToSyncker(AnimNodeBase *node, SyncMember::KIND kind, const char *syncker_name)
	{
		int index = -1;
		if (syncker_name_to_index.contains(syncker_name))
		{
			index = syncker_name_to_index[syncker_name];
		} else
		{
			index = synckers.size();
			synckers.append();
			syncker_name_to_index.append(syncker_name, index);
		}

		// Avoid duplicates
		auto &members = synckers[index].members;
		for (const auto &m : members)
			if (m.node == node) return index;

		SyncMember new_member;
		new_member.kind = kind;
		new_member.node = node;
		members.append(new_member);
		return index;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Base
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeBase
{
	enum class TYPE
	{
		UNKNOWN = 0,
		FINAL_POSE,
		ANIMATION_POSE,
		ANIMATION_PLAYER,
		ANIMATION_PLAYER_SYNC,
		BLEND,
		MAKE_ADDITIVE,
		APPLY_ADDITIVE,
		BLEND_SPACE_2D,
		BLEND_SPACE_2D_SYNC,
		STATE,
		STATE_MACHINE,
		BRANCH,
	};

	AnimNodeBase(TYPE type) : node_type(type) {}
	virtual ~AnimNodeBase() = default;

	// internal
	UGUID runtime_guid;
	int pose_layer{-1};
	bool need_update{false};
	bool need_evaluate{false};
	float accumulated_weight{0.0f};
	TYPE node_type;

	Transform root_motion_delta;

	virtual void grabConstPoseLayer(AnimGraphContext & /*context*/) {}

	virtual void update(AnimGraphContext & /*context*/) {}
	virtual void evaluate(AnimGraphContext & /*context*/) {}

	bool takeAndClearNeedUpdate()
	{
		bool value = need_update;
		need_update = false;
		return value;
	}

	bool takeAndClearNeedEvaluate()
	{
		bool value = need_evaluate;
		need_evaluate = false;
		return value;
	}

	int getSafePoseLayer(AnimGraphContext &context)
	{
		if (pose_layer != -1)
			return pose_layer;

		// Fallback to bind pose for invalid pose_layer (silent - caller is responsible for diagnostics)
		int fallback_layer = context.getTmpPoseLayer();
		context.skeleton_pose->resetLayerToBindPose(fallback_layer);
		return fallback_layer;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Pose Link Base
///////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename AnimNodeType>
struct PoseLinkBase
{
	using NodeType = AnimNodeType;

	AnimNodeType *node{nullptr};

	void updateOutputPose(AnimGraphContext &context, float child_weight)
	{
		if (node != nullptr)
		{
			node->accumulated_weight = max(node->accumulated_weight, child_weight);
			node->update(context);
		}
	}

	void evaluateOutputPose(AnimGraphContext &context)
	{
		if (node != nullptr)
			node->evaluate(context);
	}

	int getOutputPose(AnimGraphContext &context) const
	{
		if (node != nullptr && node->pose_layer != -1)
			return node->pose_layer;

		// Fallback to bind pose for invalid nodes (silent - caller is responsible for diagnostics)
		int fallback_layer = context.getTmpPoseLayer();
		context.skeleton_pose->resetLayerToBindPose(fallback_layer);
		return fallback_layer;
	}

	// Null-safe root motion access
	Transform getRootMotionDelta() const
	{
		return node ? node->root_motion_delta : transform_identity;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Pose Link
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct PoseLink: PoseLinkBase<AnimNodeBase>
{};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Final Pose
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeFinalPose: AnimNodeBase
{
	AnimNodeFinalPose() : AnimNodeBase(TYPE::FINAL_POSE) {}

	// internal
	PoseLink link;

	void grabConstPoseLayer(AnimGraphContext &/*context*/) override { pose_layer = 0; }

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		accumulated_weight = 1.0f;
		link.updateOutputPose(context, accumulated_weight);
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		if (link.node == nullptr)
		{
			// Nothing connected - keep bind pose, skip evaluation
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		link.evaluateOutputPose(context);
		context.skeleton_pose->copyLayer(pose_layer, link.getOutputPose(context));

		if (context.isActiveRootMotion())
		{
			root_motion_delta = link.getRootMotionDelta();
			context.root_motion_delta = root_motion_delta;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Animation Pose
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeAnimationPose: AnimNodeBase
{
	AnimNodeAnimationPose() : AnimNodeBase(TYPE::ANIMATION_POSE) {}

	// read / write prop
	UGUID asset_guid;
	float frame{0.0f};

	void grabConstPoseLayer(AnimGraphContext &context) override { pose_layer = context.skeleton_pose->addLayer(); }

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		float num_frames = itof(context.skeleton_pose->getLayerNumFrames(pose_layer));
		frame = clamp(frame, 0.0f, max(0.0f, num_frames - 1.0f));
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		context.skeleton_pose->setLayerFrame(pose_layer, frame);
	}

	void init(AnimGraphContext &context, const UGUID &in_asset_guid)
	{
		asset_guid = in_asset_guid;
		context.skeleton_pose->setLayerAnimationFileGUID(pose_layer, asset_guid);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Animation Player
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeAnimationPlayerBase: AnimNodeBase
{
	AnimNodeAnimationPlayerBase(TYPE type) : AnimNodeBase(type) {}

	// read / write prop
	UGUID asset_guid;
	float speed{1.0f};

	// read only prop
	float duration{0.0f};
	float current_time{0.0f};

	// internal
	bool was_inactive{false};
	float previous_time{0.0f};
	bool is_time_forward{true};

	// TELEPORT: jump to target, no root motion from jump
	void teleportTime(float t)
	{
		current_time = t;
		previous_time = t;
	}

	// ADVANCE: normal progression, produces root motion delta
	void advanceTime(float new_time, bool is_forward)
	{
		previous_time = current_time;
		current_time = new_time;
		is_time_forward = is_forward;
		// Enforce direction: clamp micro-drift from float precision
		float diff = current_time - previous_time;
		if (is_time_forward && diff < 0.0f && diff > -0.01f)
			previous_time = current_time;
		else if (!is_time_forward && diff > 0.0f && diff < 0.01f)
			previous_time = current_time;
	}

	// Clamp previous_time when syncker-assigned time contradicts leader direction.
	// Used ONLY by PlayerSync::applyNewTime() - BS2DSync drift protection.
	void clampPrevTimeIfDrift(float new_time, bool is_forward)
	{
		float diff = new_time - previous_time;
		if (is_forward && diff < 0.0f && diff > -0.01f)
			previous_time = new_time;
		else if (!is_forward && diff > 0.0f && diff < 0.01f)
			previous_time = new_time;
	}

	void grabConstPoseLayer(AnimGraphContext &context) override { pose_layer = context.skeleton_pose->addLayer(); }

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;

		context.skeleton_pose->setLayerTime(pose_layer, current_time);

		if (context.skeleton_pose->isLayerAnimationRootMotionPresent(pose_layer) && context.isActiveRootMotion())
		{
			// safety net: clamp backward time jumps that slipped through teleportTime/advanceTime.
			// If this triggers, a call site is missing the proper primitive.
			float safe_prev = previous_time;
			if (Consts::EPS < duration)
			{
				if (is_time_forward && previous_time > current_time)
				{
					float fwd_path = duration - previous_time + current_time;
					if (fwd_path > (previous_time - current_time))
						safe_prev = current_time;
				} else if (!is_time_forward && current_time > previous_time)
				{
					float bwd_path = duration - current_time + previous_time;
					if (bwd_path > (current_time - previous_time))
						safe_prev = current_time;
				}
			}

			root_motion_delta = context.skeleton_pose->extractLayerRootMotionByTimes(pose_layer, safe_prev, current_time, is_time_forward);
			context.skeleton_pose->removeLayerRootMotion(pose_layer);
		}
	}
};

struct AnimNodeAnimationPlayer: AnimNodeAnimationPlayerBase
{
	AnimNodeAnimationPlayer() : AnimNodeAnimationPlayerBase(TYPE::ANIMATION_PLAYER) {}

	// read / write prop
	bool is_loop{false};

	int syncker_index{-1};
	AnimSyncker::ROLE syncker_role{AnimSyncker::ROLE::WEIGHTED_LEADER_AND_FOLLOWER};

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;

		// no manual previous_time = current_time here - advanceTime/teleportTime handle it
		if (was_inactive)
		{
			was_inactive = false;
			syncToGroup(context, true); // before_update: other nodes may still have need_update=true
			teleportTime(current_time); // no delta on reactivation
		}

		if (syncker_index == -1)
			context.self_timed_players.append(this);
	}

	void nextTime(AnimGraphContext &context)
	{
		is_time_forward = (0.0f <= speed);
		float new_time = current_time;
		getNextTime(context.ifps, new_time);
		advanceTime(new_time, is_time_forward);
	}

	void getNextTime(float ifps, float &out_next_time) const
	{
		out_next_time = current_time + ifps * speed;
		if (is_loop && Consts::EPS < duration)
		{
			if (out_next_time < 0.0f || duration < out_next_time)
				out_next_time = fmod(fmod(out_next_time, duration) + duration, duration);
		} else
		{
			out_next_time = clamp(out_next_time, 0.0f, duration);
		}
	}

	// Set correct current_time when becoming active (reactivation or state entry)
	// before_update=true: called before node update(), need_update is stale - use wasInactive() filter
	void syncToGroup(AnimGraphContext &context, bool before_update = false)
	{
		if (syncker_index != -1 && context.synckers[syncker_index].hasOtherActiveMembers(this))
		{
			// try marker sync first, fallback to normalized time
			auto &syncker = context.synckers[syncker_index];
			syncker.rebuildCommonHashes(context, before_update);
			if (!syncker.syncTimeByMarkers(context, this, before_update))
				current_time = syncker.normalized_time * duration;
		} else
		{
			current_time = 0.0f;
			if (syncker_index != -1)
				context.synckers[syncker_index].normalized_time = 0.0f;
		}
	}

	void init(AnimGraphContext &context, const UGUID &in_asset_guid, float in_speed, bool in_is_loop)
	{
		asset_guid = in_asset_guid;
		speed = in_speed;
		context.skeleton_pose->setLayerAnimationFileGUID(pose_layer, asset_guid);
		duration = max(0.0f, context.skeleton_pose->getLayerDuration(pose_layer));
		is_loop = in_is_loop;
	}
};

// Intersect marker hashes from a pose layer into accumulator.
// Returns false if intersection became empty (caller should early-exit).
UNIGINE_INLINE bool intersectLayerMarkers(const NodeSkeletonPosePtr &skeleton_pose, int pose_layer, Vector<unsigned int> &hashes, bool &first)
{
	int num = skeleton_pose->getLayerNumMarkerSyncs(pose_layer);
	if (num == 0)
	{
		hashes.clear();
		return false;
	}
	if (first)
	{
		for (int i = 0; i < num; i++)
			hashes.appendUnique(skeleton_pose->getLayerMarkerSyncNameHash(pose_layer, i));
		first = false;
	} else
	{
		for (int i = hashes.size() - 1; 0 <= i; i--)
		{
			bool found = false;
			for (int j = 0; j < num; j++)
			{
				if (hashes[i] == skeleton_pose->getLayerMarkerSyncNameHash(pose_layer, j))
				{
					found = true;
					break;
				}
			}
			if (found == false)
				hashes.remove(i);
		}
	}
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Animation Player Sync
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeAnimationPlayerSync: AnimNodeAnimationPlayerBase
{
	AnimNodeAnimationPlayerSync() : AnimNodeAnimationPlayerBase(TYPE::ANIMATION_PLAYER_SYNC) {}

	// Apply new time from external source (BS2DSync or syncker)
	void applyNewTime(float in_new_time, bool in_is_forward)
	{
		clampPrevTimeIfDrift(in_new_time, in_is_forward);
		current_time = in_new_time;
		is_time_forward = in_is_forward;
	}

	void update(AnimGraphContext &/*context*/) override
	{
		if (takeAndClearNeedUpdate() == false) return;

		previous_time = current_time;
		if (was_inactive)
		{
			was_inactive = false;
			previous_time = current_time; // no delta on reactivation
		}
	}

	void init(AnimGraphContext &context, const UGUID &in_asset_guid, float in_speed)
	{
		asset_guid = in_asset_guid;
		speed = in_speed;
		context.skeleton_pose->setLayerAnimationFileGUID(pose_layer, asset_guid);
		duration = max(0.0f, context.skeleton_pose->getLayerDuration(pose_layer));
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Sync Pose Link
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct SyncPoseLink: PoseLinkBase<AnimNodeAnimationPlayerSync>
{};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Make Additive
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeMakeAdditive: AnimNodeBase
{
	AnimNodeMakeAdditive() : AnimNodeBase(TYPE::MAKE_ADDITIVE) {}

	// internal
	PoseLink link_base_pose;
	PoseLink link_ref_pose;

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		link_base_pose.updateOutputPose(context, accumulated_weight);
		link_ref_pose.updateOutputPose(context, accumulated_weight);
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		if (link_base_pose.node == nullptr || link_ref_pose.node == nullptr)
		{
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		link_base_pose.evaluateOutputPose(context);
		link_ref_pose.evaluateOutputPose(context);

		pose_layer = context.getTmpPoseLayer();
		context.skeleton_pose->inverseLayer(pose_layer, link_ref_pose.getOutputPose(context));
		context.skeleton_pose->mulLayer(pose_layer, pose_layer, link_base_pose.getOutputPose(context));

		if (context.isActiveRootMotion())
			root_motion_delta = link_base_pose.getRootMotionDelta();
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Apply Additive
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeApplyAdditive: AnimNodeBase
{
	AnimNodeApplyAdditive() : AnimNodeBase(TYPE::APPLY_ADDITIVE) {}

	// read / write prop
	float weight{0.0f};

	// internal
	PoseLink link_base_pose;
	PoseLink link_additive;

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		weight = saturate(weight);
		link_base_pose.updateOutputPose(context, accumulated_weight);
		if (Consts::EPS < weight)
			link_additive.updateOutputPose(context, accumulated_weight * weight);
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		if (link_base_pose.node == nullptr)
		{
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		link_base_pose.evaluateOutputPose(context);
		if (Consts::EPS < weight && link_additive.node)
		{
			link_additive.evaluateOutputPose(context);
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->mulLayer(pose_layer, link_base_pose.getOutputPose(context), link_additive.getOutputPose(context), weight);
		} else
		{
			pose_layer = link_base_pose.getOutputPose(context);
		}

		if (context.isActiveRootMotion())
			root_motion_delta = link_base_pose.getRootMotionDelta();
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Blend
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeBlend: AnimNodeBase
{
	AnimNodeBlend() : AnimNodeBase(TYPE::BLEND) {}

	// read / write prop
	float weight{0.0f};
	int blend_mask{-1};

	// internal
	PoseLink link_0;
	PoseLink link_1;

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		weight = saturate(weight);
		if (weight < Consts::EPS)
		{
			link_0.updateOutputPose(context, accumulated_weight);
		} else if (blend_mask == -1 && (1.0f - Consts::EPS) < weight)
		{
			link_1.updateOutputPose(context, accumulated_weight);
		} else
		{
			link_0.updateOutputPose(context, accumulated_weight * (blend_mask == -1 ? (1.0f - weight) : 1.0f));
			link_1.updateOutputPose(context, accumulated_weight * weight);
		}
	}
	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;

		// Null-safe: if both inputs disconnected, use bind pose
		if (link_0.node == nullptr && link_1.node == nullptr)
		{
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		// If only one input connected, use it directly (the other falls back to bind pose via getOutputPose)
		if (weight < Consts::EPS || link_1.node == nullptr)
		{
			link_0.evaluateOutputPose(context);
			pose_layer = link_0.getOutputPose(context);

			if (context.isActiveRootMotion())
				root_motion_delta = link_0.getRootMotionDelta();

		} else if (link_0.node == nullptr || (blend_mask == -1 && (1.0f - Consts::EPS) < weight))
		{
			link_1.evaluateOutputPose(context);
			pose_layer = link_1.getOutputPose(context);

			if (context.isActiveRootMotion())
				root_motion_delta = link_1.getRootMotionDelta();
		} else
		{
			link_0.evaluateOutputPose(context);
			link_1.evaluateOutputPose(context);

			pose_layer = context.getTmpPoseLayer();
			if (blend_mask == -1)
			{
				context.skeleton_pose->lerpLayer(pose_layer, link_0.getOutputPose(context), link_1.getOutputPose(context), weight);

				if (context.isActiveRootMotion())
					root_motion_delta = blendTransform(link_0.getRootMotionDelta(), link_1.getRootMotionDelta(), weight);
			} else
			{
				context.skeleton_pose->lerpLayerByMask(pose_layer, link_0.getOutputPose(context), link_1.getOutputPose(context), blend_mask, weight);

				if (context.isActiveRootMotion())
				{
					float joint_mask = context.skeleton_pose->getSkeleton()->getBlendMaskInfluence(blend_mask, 0);
					root_motion_delta = blendTransform(link_0.getRootMotionDelta(), link_1.getRootMotionDelta(), weight * joint_mask);
				}
			}
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Branch - evaluates only one of two pose branches based on condition
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeBranch: AnimNodeBase
{
	AnimNodeBranch(): AnimNodeBase(TYPE::BRANCH) {}

	// read / write prop
	bool condition{false};

	// internal
	PoseLink link_true;
	PoseLink link_false;

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		if (condition)
			link_true.updateOutputPose(context, accumulated_weight);
		else
			link_false.updateOutputPose(context, accumulated_weight);
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;

		PoseLink &active = condition ? link_true : link_false;
		if (active.node == nullptr)
		{
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		active.evaluateOutputPose(context);
		pose_layer = active.getOutputPose(context);

		if (context.isActiveRootMotion())
			root_motion_delta = active.getRootMotionDelta();
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Blend Space 2D
///////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename Link>
struct AnimNodeBlendSpace2DBase: AnimNodeBase
{
	AnimNodeBlendSpace2DBase(TYPE type) : AnimNodeBase(type) {}

	using LinkType = Link;
	using NodeType = typename Link::NodeType;

	// read / write prop
	vec2 blend_point;

	enum class MODE
	{
		CARTESIAN = 0,
		POLAR
	};
	MODE mode{MODE::CARTESIAN};

	bool use_smoothed_weights{false};
	float weight_smoothing_speed{1.0f};

	// internal
	Vector<Link> links;
	Vector<WeightPoint2D> points;
	Vector<float> smoothed_weights;

	void addSample(NodeType *linked_node, const vec2 &anchor)
	{
		Link &link = this->links.append();
		link.node = linked_node;
		this->points.append({anchor});
		smoothed_weights.append(0.0f);
	}

	// Snap smoothed_weights to current blend_point without interpolation
	void snapWeights()
	{
		if (this->points.empty())
			return;
		if (mode == MODE::CARTESIAN)
			blend2DCartesianWeights(blend_point, this->points);
		else
			blend2DPolarWeights(blend_point, this->points);
		for (int i = 0; i < smoothed_weights.size(); i++)
			smoothed_weights[i] = this->points[i].normalized_weight;
	}

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;

		if (this->links.empty())
			return;

		if (this->links.size() == 1)
		{
			this->points[0].normalized_weight = 1.0f;
			smoothed_weights[0] = 1.0f;
			updateLinks(context);
			return;
		}

		if (mode == MODE::CARTESIAN)
			blend2DCartesianWeights(blend_point, this->points);
		else
			blend2DPolarWeights(blend_point, this->points);

		if (use_smoothed_weights)
		{
			const float max_delta = context.ifps * weight_smoothing_speed;
			float total_weight = 0.0f;
			for (int i = 0; i < smoothed_weights.size(); i++)
			{
				smoothed_weights[i] = move_towards(smoothed_weights[i], this->points[i].normalized_weight, max_delta);
				total_weight += smoothed_weights[i];
			}
			if (Consts::EPS < total_weight)
			{
				for (int i = 0; i < smoothed_weights.size(); i++)
					smoothed_weights[i] /= total_weight;
			}
		} else
		{
			for (int i = 0; i < smoothed_weights.size(); i++)
				smoothed_weights[i] = this->points[i].normalized_weight;
		}

		updateLinks(context);
	}
	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;

		for (int i = 0; i < links.size(); i++)
		{
			if ((1.0f - Consts::EPS) < smoothed_weights[i])
			{
				links[i].evaluateOutputPose(context);
				pose_layer = links[i].getOutputPose(context);

				if (context.isActiveRootMotion())
					root_motion_delta = links[i].getRootMotionDelta();

				return;
			}
		}

		pose_layer = context.getTmpPoseLayer();

		float total_weight = 0.0f;
		for (int i = 0; i < links.size(); i++)
		{
			if (smoothed_weights[i] < Consts::EPS)
				continue;
			links[i].evaluateOutputPose(context);

			total_weight += smoothed_weights[i];
			context.skeleton_pose->lerpLayer(pose_layer, pose_layer, links[i].getOutputPose(context), smoothed_weights[i] / total_weight);

			if (context.isActiveRootMotion())
				root_motion_delta = blendTransform(root_motion_delta, links[i].getRootMotionDelta(), smoothed_weights[i] / total_weight);
		}

		// If all weights are zero, use bind pose as fallback
		if (total_weight < Consts::EPS)
		{
			context.skeleton_pose->resetLayerToBindPose(pose_layer);

			if (context.isActiveRootMotion())
				root_motion_delta = transform_identity;

			// all weights zero - bind-pose is the correct result, no warning needed
		}
	}

protected:
	virtual void updateLinks(AnimGraphContext &context)
	{
		for (int i = 0; i < this->links.size(); i++)
		{
			if (Consts::EPS < smoothed_weights[i])
				this->links[i].updateOutputPose(context, this->accumulated_weight * this->smoothed_weights[i]);
		}
	}

	float move_towards(float current, float target, float max_delta)
	{
		float delta = target - current;
		if (abs(delta) <= max_delta)
			return target;
		return current + sign(delta) * max_delta;
	}
};

struct AnimNodeBlendSpace2D: AnimNodeBlendSpace2DBase<PoseLink>
{
	AnimNodeBlendSpace2D() : AnimNodeBlendSpace2DBase(TYPE::BLEND_SPACE_2D) {}

	bool was_inactive{false};

	void updateLinks(AnimGraphContext &context) override
	{
		if (was_inactive)
		{
			was_inactive = false;
			snapWeights();
		}
		AnimNodeBlendSpace2DBase::updateLinks(context);
	}
};

struct AnimNodeBlendSpace2DSync: AnimNodeBlendSpace2DBase<SyncPoseLink>
{
	AnimNodeBlendSpace2DSync() : AnimNodeBlendSpace2DBase(TYPE::BLEND_SPACE_2D_SYNC) {}

	// read only prop
	float duration{0.0f};
	float normalized_time{0.0f};
	float current_time{0.0f};
	float speed{1.0f};

	int syncker_index{-1};
	AnimSyncker::ROLE syncker_role{AnimSyncker::ROLE::WEIGHTED_LEADER_AND_FOLLOWER};

	// internal
	bool was_inactive{false};

	// Index of child with highest smoothed_weight (-1 if none)
	int getDominantChildIndex() const
	{
		int best = -1;
		float max_w = -1.0f;
		for (int i = 0; i < links.size(); i++)
		{
			if (links[i].node && max_w < smoothed_weights[i])
			{
				max_w = smoothed_weights[i];
				best = i;
			}
		}
		return best;
	}

	void applyNormalizedTime(float in_normalized_time, bool in_is_forward)
	{
		normalized_time = in_normalized_time;
		current_time = normalized_time * duration;
		for (int i = 0; i < links.size(); i++)
		{
			if (links[i].node == nullptr)
				continue;
			links[i].node->applyNewTime(normalized_time * links[i].node->duration, in_is_forward);
		}
	}

	// Set correct normalized_time when becoming active (reactivation or state entry)
	// before_update=true: called before node update(), need_update is stale - use wasInactive() filter
	void syncToGroup(AnimGraphContext &context, bool before_update = false)
	{
		if (syncker_index != -1 && context.synckers[syncker_index].hasOtherActiveMembers(this))
		{
			// try marker sync first, fallback to normalized time (same as AnimNodeAnimationPlayer)
			auto &syncker = context.synckers[syncker_index];
			syncker.rebuildCommonHashes(context, before_update);
			if (!syncker.syncTimeByMarkers(context, this, before_update))
				normalized_time = syncker.normalized_time;
		} else
		{
			normalized_time = 0.0f;
			if (syncker_index != -1)
				context.synckers[syncker_index].normalized_time = 0.0f;
		}
		current_time = 0.0f; // recalculated in updateLinks from normalized_time * duration
	}

	// propagate normalized_time to children PlayerSync nodes via teleportTime
	void syncChildrenTime()
	{
		for (int i = 0; i < links.size(); i++)
		{
			if (links[i].node == nullptr)
				continue;
			links[i].node->teleportTime(normalized_time * links[i].node->duration);
		}
	}

protected:
	void updateLinks(AnimGraphContext &context) override
	{
		if (was_inactive)
		{
			was_inactive = false;
			snapWeights();
			syncToGroup(context, true); // before_update: other nodes may still have need_update=true
			syncChildrenTime();
		}

		duration = 0.0f;
		speed = 0.0f;
		for (int i = 0; i < links.size(); i++)
		{
			auto *n = links[i].node;
			if (n == nullptr)
				continue;
			duration += n->duration * smoothed_weights[i];
			speed += n->speed * smoothed_weights[i];
		}

		current_time = normalized_time * duration;
		current_time += context.ifps * speed;
		bool is_forward = (0.0f <= speed);
		if (Consts::EPS < duration)
		{
			current_time = fmod(fmod(current_time, duration) + duration, duration);
			normalized_time = current_time / duration;
		}

		for (int i = 0; i < links.size(); i++)
		{
			auto *n = links[i].node;
			if (n == nullptr || smoothed_weights[i] < Consts::EPS)
				continue;
			if (n->was_inactive)
				n->teleportTime(normalized_time * n->duration); // DR-013
			links[i].updateOutputPose(context, this->accumulated_weight * smoothed_weights[i]);
			n->applyNewTime(normalized_time * n->duration, is_forward);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// SyncMember inline implementations (need complete AnimNodeAnimationPlayer & BS2DSync)
///////////////////////////////////////////////////////////////////////////////////////////////////////
UNIGINE_INLINE float SyncMember::getDuration() const
{
	if (kind == KIND::PLAYER)
		return static_cast<const AnimNodeAnimationPlayer *>(node)->duration;
	return static_cast<const AnimNodeBlendSpace2DSync *>(node)->duration;
}

UNIGINE_INLINE float SyncMember::getCurrentTime() const
{
	if (kind == KIND::PLAYER)
		return static_cast<const AnimNodeAnimationPlayer *>(node)->current_time;
	return static_cast<const AnimNodeBlendSpace2DSync *>(node)->current_time;
}

UNIGINE_INLINE float SyncMember::getAccumulatedWeight() const { return node->accumulated_weight; }

UNIGINE_INLINE bool SyncMember::wasInactive() const
{
	if (kind == KIND::PLAYER)
		return static_cast<const AnimNodeAnimationPlayer *>(node)->was_inactive;
	return static_cast<const AnimNodeBlendSpace2DSync *>(node)->was_inactive;
}

UNIGINE_INLINE SyncMember::ROLE SyncMember::getRole() const
{
	if (kind == KIND::PLAYER)
		return static_cast<const AnimNodeAnimationPlayer *>(node)->syncker_role;
	return static_cast<const AnimNodeBlendSpace2DSync *>(node)->syncker_role;
}

UNIGINE_INLINE bool SyncMember::needsUpdate() const { return node->need_update; }

UNIGINE_INLINE bool SyncMember::isLoop() const
{
	if (kind == KIND::PLAYER)
		return static_cast<const AnimNodeAnimationPlayer *>(node)->is_loop;
	return true; // BS2DSync is always loop
}

UNIGINE_INLINE float SyncMember::getLeaderNextTime(float ifps) const
{
	if (kind == KIND::PLAYER)
	{
		auto *p = static_cast<AnimNodeAnimationPlayer *>(node);
		float next = 0.0f;
		p->getNextTime(ifps, next);
		return next;
	}
	// BS2DSync: time already advanced in updateLinks(), just read it
	return static_cast<const AnimNodeBlendSpace2DSync *>(node)->current_time;
}

UNIGINE_INLINE bool SyncMember::isTimeForward() const
{
	if (kind == KIND::PLAYER)
		return (0.0f <= static_cast<const AnimNodeAnimationPlayer *>(node)->speed);
	return (0.0f <= static_cast<const AnimNodeBlendSpace2DSync *>(node)->speed);
}

UNIGINE_INLINE void SyncMember::applyLeaderTime(float next_time, bool is_forward)
{
	if (kind == KIND::PLAYER)
	{
		// advanceTime sets previous=current, then current=next_time, with drift enforcement
		static_cast<AnimNodeAnimationPlayer *>(node)->advanceTime(next_time, is_forward);
	}
	// BS2DSync: noop - time already set in updateLinks(), children already received time
}

UNIGINE_INLINE void SyncMember::applyFollowerTime(float nt, bool is_forward)
{
	if (kind == KIND::PLAYER)
	{
		// advanceTime handles previous_time and drift enforcement
		auto *p = static_cast<AnimNodeAnimationPlayer *>(node);
		p->advanceTime(nt * p->duration, is_forward);
	} else
	{
		static_cast<AnimNodeBlendSpace2DSync *>(node)->applyNormalizedTime(nt, is_forward);
	}
}

UNIGINE_INLINE void SyncMember::intersectMarkers(AnimGraphContext &ctx, Vector<unsigned int> &hashes, bool &first) const
{
	if (kind == KIND::PLAYER)
	{
		intersectLayerMarkers(ctx.skeleton_pose, node->pose_layer, hashes, first);
	} else
	{
		// BS2DSync: intersect ALL active children with non-zero weight
		auto *bs = static_cast<const AnimNodeBlendSpace2DSync *>(node);
		for (int i = 0; i < bs->links.size(); i++)
		{
			if (bs->links[i].node == nullptr || bs->smoothed_weights[i] < Consts::EPS)
				continue;
			if (intersectLayerMarkers(ctx.skeleton_pose, bs->links[i].node->pose_layer, hashes, first) == false)
				return;
		}
	}
}

UNIGINE_INLINE bool SyncMember::getMarkerSyncInterval(AnimGraphContext &ctx, float time, unsigned int &ph, unsigned int &nh, float &r, const Vector<unsigned int> &h) const
{
	if (kind == KIND::PLAYER)
	{
		auto *p = static_cast<const AnimNodeAnimationPlayer *>(node);
		return ctx.skeleton_pose->getLayerMarkerSyncInterval(p->pose_layer, time, p->is_loop, ph, nh, r, h);
	}
	// BS2DSync: use dominant child
	auto *bs = static_cast<const AnimNodeBlendSpace2DSync *>(node);
	int ci = bs->getDominantChildIndex();
	if (ci < 0)
		return false;
	const auto *cn = bs->links[ci].node;
	if (cn->duration < Consts::EPS)
		return false;
	float nt = (Consts::EPS < bs->duration) ? (time / bs->duration) : 0.0f;
	float child_time = nt * cn->duration;
	return ctx.skeleton_pose->getLayerMarkerSyncInterval(cn->pose_layer, child_time, true, ph, nh, r, h);
}

UNIGINE_INLINE void SyncMember::applyMarkerTime(AnimGraphContext &ctx, unsigned int ph, unsigned int nh, float r, const Vector<unsigned int> &h, bool is_forward, float fallback_nt)
{
	if (kind == KIND::PLAYER)
	{
		// advanceTime handles previous_time and drift enforcement
		auto *p = static_cast<AnimNodeAnimationPlayer *>(node);
		float new_time = ctx.skeleton_pose->getLayerTimeFromMarkerSyncInterval(p->pose_layer, ph, nh, r, p->current_time, p->is_loop, h);
		p->advanceTime(new_time, is_forward);
	} else
	{
		auto *bs = static_cast<AnimNodeBlendSpace2DSync *>(node);
		int ci = bs->getDominantChildIndex();
		auto *fn = (0 <= ci) ? bs->links[ci].node : nullptr;
		if (fn != nullptr && Consts::EPS < fn->duration)
		{
			float child_time = ctx.skeleton_pose->getLayerTimeFromMarkerSyncInterval(fn->pose_layer, ph, nh, r, bs->normalized_time * fn->duration, true, h);
			bs->applyNormalizedTime(child_time / fn->duration, is_forward);
		} else
		{
			bs->applyNormalizedTime(fallback_nt, is_forward);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// AnimSyncker inline implementations
///////////////////////////////////////////////////////////////////////////////////////////////////////
UNIGINE_INLINE bool AnimSyncker::hasOtherActiveMembers(const AnimNodeBase *exclude) const
{
	for (const auto &m : members)
		if (m.node != exclude && m.wasInactive() == false) return true;
	return false;
}

UNIGINE_INLINE void AnimSyncker::rebuildCommonHashes(AnimGraphContext &context, bool before_update)
{
	common_hashes.clear();
	bool first = true;
	for (const auto &m : members)
	{
		// before_update: need_update is true for ALL nodes, use wasInactive() to find active ones
		if (before_update ? m.wasInactive() : m.needsUpdate())
			continue;
		m.intersectMarkers(context, common_hashes, first);
		if (common_hashes.empty() && first == false)
			return;
	}
}

UNIGINE_INLINE bool AnimSyncker::syncTimeByMarkers(AnimGraphContext &context, AnimNodeBase *target_node, bool before_update)
{
	if (sync_method != SYNC_METHOD::MARKER_SYNC || common_hashes.size() < 2)
		return false;
	SyncMember *target = nullptr;
	for (auto &m : members)
		if (m.node == target_node) { target = &m; break; }
	if (target == nullptr)
		return false;

	for (auto &m : members)
	{
		// before_update: need_update is true for ALL nodes, use wasInactive() to find active ones
		if ((before_update ? m.wasInactive() : m.needsUpdate()) || m.node == target_node)
			continue;
		unsigned int ph = 0, nh = 0;
		float r = 0.0f;
		if (m.getMarkerSyncInterval(context, m.getCurrentTime(), ph, nh, r, common_hashes))
		{
			target->applyMarkerTime(context, ph, nh, r, common_hashes, true, normalized_time);
			return true;
		}
	}
	return false;
}

UNIGINE_INLINE void AnimSyncker::update(AnimGraphContext &context)
{
	// 1. Find leader
	int new_leader_idx = -1;
	float max_weight = -1.0f;
	for (int i = 0; i < members.size(); i++)
	{
		auto &m = members[i];
		if (m.needsUpdate()) continue;

		if (m.getRole() == ROLE::LEADER)
		{
			new_leader_idx = i;
			break;
		}
		if (m.getRole() == ROLE::WEIGHTED_LEADER_AND_FOLLOWER && max_weight < m.getAccumulatedWeight())
		{
			max_weight = m.getAccumulatedWeight();
			new_leader_idx = i;
		}
	}

	if (new_leader_idx == -1)
	{
		is_valid_leader = false;
		max_weight = -1.0f;
		for (int i = 0; i < members.size(); i++)
		{
			auto &m = members[i];
			if (m.needsUpdate() || m.getRole() != ROLE::FOLLOWER) continue;
			if (max_weight < m.getAccumulatedWeight())
			{
				max_weight = m.getAccumulatedWeight();
				new_leader_idx = i;
			}
		}
	} else
	{
		// Preventing time lag after a valid leader has reappeared or changed
		if (is_valid_leader == false || leader_index != new_leader_idx)
		{
			rebuildCommonHashes(context);
			if (!syncTimeByMarkers(context, members[new_leader_idx].node))
				members[new_leader_idx].applyFollowerTime(normalized_time, true);
		}
		is_valid_leader = true;
	}

	leader_index = new_leader_idx;
	if (leader_index == -1)
		return;
	if (members[leader_index].getDuration() < Consts::EPS)
		return;

	auto &leader = members[leader_index];

	// 2. Get next time from leader
	float next_time = leader.getLeaderNextTime(context.ifps);
	bool leader_forward = leader.isTimeForward();
	normalized_time = next_time / leader.getDuration();

	// 3. Marker sync
	bool use_markers = false;
	unsigned int prev_hash = 0, next_hash = 0;
	float ratio = 0.0f;
	if (sync_method == SYNC_METHOD::MARKER_SYNC)
	{
		rebuildCommonHashes(context);
		if (2 <= common_hashes.size())
		{
			use_markers = leader.getMarkerSyncInterval(context, next_time, prev_hash, next_hash, ratio, common_hashes);
			if (!use_markers)
			{
				for (int i = 0; i < members.size(); i++)
				{
					if (i == leader_index || members[i].needsUpdate()) continue;
					if (members[i].getMarkerSyncInterval(context, members[i].getCurrentTime(), prev_hash, next_hash, ratio, common_hashes))
					{
						use_markers = true;
						break;
					}
				}
			}
		}
	}

	// 4. Apply to all members
	for (int i = 0; i < members.size(); i++)
	{
		auto &m = members[i];
		if (m.needsUpdate()) continue;

		if (i == leader_index)
			m.applyLeaderTime(next_time, leader_forward);
		else if (use_markers)
			m.applyMarkerTime(context, prev_hash, next_hash, ratio, common_hashes, leader_forward, normalized_time);
		else
			m.applyFollowerTime(normalized_time, leader_forward);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node State
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeState: AnimNodeBase
{
	struct Transition
	{
		Transition() = default;
		Transition(const Transition &) = delete;
		Transition &operator=(const Transition &) = delete;

		Transition(Transition &&other) noexcept
			: runtime_guid(other.runtime_guid), to_state(other.to_state), duration(other.duration),
			can_be_interrupted(other.can_be_interrupted), condition_callback(other.condition_callback),
			has_enter_time(other.has_enter_time), enter_time_callback(other.enter_time_callback),
			child_transitions(std::move(other.child_transitions))
		{
			other.to_state = nullptr;
			other.condition_callback = nullptr;
			other.enter_time_callback = nullptr;
		}
		Transition &operator=(Transition &&other) noexcept
		{
			if (this != &other)
			{
				delete condition_callback;
				delete enter_time_callback;

				to_state = other.to_state;
				duration = other.duration;
				can_be_interrupted = other.can_be_interrupted;
				condition_callback = other.condition_callback;
				runtime_guid = other.runtime_guid;
				has_enter_time = other.has_enter_time;
				enter_time_callback = other.enter_time_callback;
				child_transitions = std::move(other.child_transitions);

				other.to_state = nullptr;
				other.condition_callback = nullptr;
				other.enter_time_callback = nullptr;
			}
			return *this;
		}

		~Transition()
		{
			delete condition_callback;
			delete enter_time_callback;
		}

		UGUID runtime_guid;
		AnimNodeState *to_state{nullptr};
		float duration{0.2f};
		bool can_be_interrupted{false};

		using CallbackType = CallbackBase2<AnimGraphContext const &, bool &>;
		CallbackType *condition_callback{nullptr};

		// Optional: compute normalized start time for destination state (0.0 - 1.0)
		bool has_enter_time{false};
		using EnterTimeCallbackType = CallbackBase2<AnimGraphContext const &, float &>;
		EnterTimeCallbackType *enter_time_callback{nullptr};

		// non-owning pointers, lifetime managed by graph class
		Vector<Transition *> child_transitions;

		// Resolve transition tree (Condition nodes form a routing tree via child_transitions).
		// out_resolved: the leaf transition that actually fired (has duration, enter_time, etc.)
		AnimNodeState *resolveTransition(const AnimGraphContext &context, const Transition **out_resolved = nullptr) const
		{
			// No callback = corrupted data, block transition
			if (condition_callback == nullptr)
				return nullptr;
			bool out_result = false;
			condition_callback->run(context, out_result);
			if (out_result == false)
				return nullptr;

			// Condition passed, check child_transitions (routing tree)
			if (child_transitions.empty() == false)
			{
				for (auto *child : child_transitions)
				{
					AnimNodeState *result = child->resolveTransition(context, out_resolved);
					if (result)
						return result;
				}
				return nullptr;
			}

			// Leaf transition: condition passed, return target
			if (out_resolved)
				*out_resolved = this;
			return to_state;
		}

		// Compute enter normalized time (0.0 - 1.0), returns -1 if not used
		float computeEnterTime(const AnimGraphContext &context) const
		{
			if (has_enter_time == false || enter_time_callback == nullptr)
				return -1.0f;
			float out_time = 0.0f;
			enter_time_callback->run(context, out_time);
			return saturate(out_time);
		}
	};

	AnimNodeState() : AnimNodeBase(TYPE::STATE) {}

	// read / write props
	String state_name;

	// internal
	PoseLink link_pose;
	Vector<Transition *> transitions;

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;
		link_pose.updateOutputPose(context, accumulated_weight);
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		if (link_pose.node == nullptr)
		{
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->resetLayerToBindPose(pose_layer);
			return;
		}
		link_pose.evaluateOutputPose(context);
		pose_layer = link_pose.getOutputPose(context);

		if (context.isActiveRootMotion())
			root_motion_delta = link_pose.getRootMotionDelta();
	}

	void onEnter(AnimGraphContext &context, float enter_normalized_time = -1.0f)
	{
		resetPlaybackNodes(link_pose.node, context);
		if (0 <= enter_normalized_time)
			applyNormalizedTime(link_pose.node, context, enter_normalized_time);
	}

	void onExit(AnimGraphContext &/*context*/) {}

	static void resetPlaybackNodes(AnimNodeBase *node, AnimGraphContext &context); // defined after AnimNodeStateMachine

	// Set normalized time (0.0 - 1.0) on playback nodes after reset
	static void applyNormalizedTime(AnimNodeBase *node, AnimGraphContext &context, float normalized_time); // defined after AnimNodeStateMachine
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node State Machine
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeStateMachine: AnimNodeBase
{
	using Transition = AnimNodeState::Transition;

	AnimNodeStateMachine() : AnimNodeBase(TYPE::STATE_MACHINE) {}

	static const AnimNodeAnimationPlayer *findRelevantPlayer(const AnimNodeBase *node)
	{
		if (node == nullptr)
			return nullptr;

		switch (node->node_type)
		{
			case TYPE::ANIMATION_PLAYER:
			{
				return static_cast<const AnimNodeAnimationPlayer *>(node);
			}
			case TYPE::STATE:
			{
				return findRelevantPlayer(static_cast<const AnimNodeState *>(node)->link_pose.node);
			}
			case TYPE::BLEND:
			{
				auto *blend = static_cast<const AnimNodeBlend *>(node);
				const AnimNodeBase *primary = (blend->weight < 0.5f) ? blend->link_0.node : blend->link_1.node;
				const AnimNodeBase *secondary = (blend->weight < 0.5f) ? blend->link_1.node : blend->link_0.node;
				auto *result = findRelevantPlayer(primary);
				return result ? result : findRelevantPlayer(secondary);
			}
			case TYPE::APPLY_ADDITIVE:
			{
				return findRelevantPlayer(static_cast<const AnimNodeApplyAdditive *>(node)->link_base_pose.node);
			}
			case TYPE::MAKE_ADDITIVE:
			{
				return findRelevantPlayer(static_cast<const AnimNodeMakeAdditive *>(node)->link_base_pose.node);
			}
			case TYPE::BRANCH:
			{
				auto *branch = static_cast<const AnimNodeBranch *>(node);
				return findRelevantPlayer(branch->condition ? branch->link_true.node : branch->link_false.node);
			}
			case TYPE::BLEND_SPACE_2D:
			{
				auto *bs = static_cast<const AnimNodeBlendSpace2D *>(node);
				const AnimNodeAnimationPlayer *best = nullptr;
				float best_w = -1.0f;
				for (int i = 0; i < bs->links.size(); i++)
				{
					if (best_w < bs->smoothed_weights[i])
					{
						auto *p = findRelevantPlayer(bs->links[i].node);
						if (p != nullptr)
						{
							best = p;
							best_w = bs->smoothed_weights[i];
						}
					}
				}
				return best;
			}
			case TYPE::STATE_MACHINE:
			{
				auto *sm = static_cast<const AnimNodeStateMachine *>(node);
				if (sm->current_state != nullptr)
					return findRelevantPlayer(sm->current_state);
				return nullptr;
			}
			default:
			{
				return nullptr;
			}
		}
	}

	struct TimeSourceInfo
	{
		float duration{0.0f};
		float current_time{0.0f};
	};

	static TimeSourceInfo findRelevantTimeSource(const AnimNodeBase *node)
	{
		if (node == nullptr)
			return {};

		switch (node->node_type)
		{
			case TYPE::ANIMATION_PLAYER:
			{
				auto *p = static_cast<const AnimNodeAnimationPlayer *>(node);
				TimeSourceInfo ts;
				ts.duration = p->duration;
				ts.current_time = p->current_time;
				return ts;
			}
			case TYPE::BLEND_SPACE_2D_SYNC:
			{
				auto *bs = static_cast<const AnimNodeBlendSpace2DSync *>(node);
				TimeSourceInfo ts;
				ts.duration = bs->duration;
				ts.current_time = bs->current_time;
				return ts;
			}
			case TYPE::STATE:
			{
				return findRelevantTimeSource(static_cast<const AnimNodeState *>(node)->link_pose.node);
			}
			case TYPE::BLEND:
			{
				auto *blend = static_cast<const AnimNodeBlend *>(node);
				const AnimNodeBase *primary = (blend->weight < 0.5f) ? blend->link_0.node : blend->link_1.node;
				const AnimNodeBase *secondary = (blend->weight < 0.5f) ? blend->link_1.node : blend->link_0.node;
				auto ts = findRelevantTimeSource(primary);
				return (Consts::EPS < ts.duration) ? ts : findRelevantTimeSource(secondary);
			}
			case TYPE::APPLY_ADDITIVE:
			{
				return findRelevantTimeSource(static_cast<const AnimNodeApplyAdditive *>(node)->link_base_pose.node);
			}
			case TYPE::MAKE_ADDITIVE:
			{
				return findRelevantTimeSource(static_cast<const AnimNodeMakeAdditive *>(node)->link_base_pose.node);
			}
			case TYPE::BRANCH:
			{
				auto *branch = static_cast<const AnimNodeBranch *>(node);
				return findRelevantTimeSource(branch->condition ? branch->link_true.node : branch->link_false.node);
			}
			case TYPE::BLEND_SPACE_2D:
			{
				auto *bs = static_cast<const AnimNodeBlendSpace2D *>(node);
				TimeSourceInfo best = {};
				float best_w = -1.0f;
				for (int i = 0; i < bs->links.size(); i++)
				{
					if (best_w < bs->smoothed_weights[i])
					{
						auto ts = findRelevantTimeSource(bs->links[i].node);
						if (Consts::EPS < ts.duration)
						{
							best = ts;
							best_w = bs->smoothed_weights[i];
						}
					}
				}
				return best;
			}
			case TYPE::STATE_MACHINE:
			{
				auto *sm = static_cast<const AnimNodeStateMachine *>(node);
				if (sm->current_state != nullptr)
					return findRelevantTimeSource(sm->current_state);
				return {};
			}
			default:
			{
				return {};
			}
		}
	}

	// read / write props
	String sm_name;

	// internal
	AnimNodeState::Transition entry_transition;
	AnimNodeState *current_state{nullptr};
	AnimNodeState *previous_state{nullptr};
	bool is_blending_previous{false};
	const AnimNodeState::Transition *active_transition{nullptr};
	float transition_time{0.0f};
	float transition_duration{0.0f};
	float transition_progress{1.0f};

	float current_state_time{0.0f};

	// entry resolve once-warning flag
	bool entry_resolve_failed{false};

	// interruption info
	int frozen_pose_layer{-1};
	bool has_frozen_pose{false};
	Transform frozen_root_motion_delta; // root motion snapshot at the moment of freezing
	bool pending_interruption{false};
	AnimNodeState *pending_target{nullptr};
	const AnimNodeState::Transition *pending_transition{nullptr};

	void grabConstPoseLayer(AnimGraphContext &context) override
	{
		frozen_pose_layer = context.skeleton_pose->addLayer();
	}

	void update(AnimGraphContext &context) override
	{
		if (takeAndClearNeedUpdate() == false) return;

		// entry point
		if (current_state == nullptr)
		{
			const Transition *resolved_tr = nullptr;
			current_state = entry_transition.resolveTransition(context, &resolved_tr);
			if (current_state == nullptr)
			{
				if (entry_resolve_failed == false)
				{
					Log::warning("AnimNodeStateMachine::update(): entry_transition resolved to nullptr (no valid entry state)\n");
					entry_resolve_failed = true;
				}
				return;
			}
			entry_resolve_failed = false;
			current_state->onEnter(context, resolved_tr->computeEnterTime(context));
			current_state_time = 0.0f;
		}

		// apply deferred interruption from previous frame
		if (pending_interruption)
		{
			has_frozen_pose = true;
			is_blending_previous = false;

			current_state->onExit(context);
			current_state = pending_target;
			active_transition = pending_transition;
			transition_time = 0.0f;
			transition_duration = pending_transition->duration;
			current_state->onEnter(context, pending_transition->computeEnterTime(context));
			current_state_time = 0.0f;

			pending_interruption = false;
			pending_target = nullptr;
			pending_transition = nullptr;
		}

		// state machine context
		context.pushSMContext();

		current_state_time += context.ifps;
		context.sm.state_time = current_state_time;

		context.sm.anim_ended = false;
		const auto *relevant_player = findRelevantPlayer(current_state);
		if (relevant_player != nullptr)
			context.sm.anim_ended = ((relevant_player->is_loop == false) && (Consts::EPS < relevant_player->duration) && ((relevant_player->duration - Consts::EPS) <= relevant_player->current_time));

		context.sm.anim_time_remaining = 0.0f;
		context.sm.anim_time_remaining_fraction = 0.0f;
		context.sm.anim_length = 0.0f;
		context.sm.anim_time = 0.0f;
		context.sm.anim_time_fraction = 0.0f;
		const auto ts = findRelevantTimeSource(current_state);
		if (Consts::EPS < ts.duration)
		{
			context.sm.anim_time_remaining = max(0.0f, ts.duration - ts.current_time);
			context.sm.anim_time_remaining_fraction = context.sm.anim_time_remaining / ts.duration;
			context.sm.anim_length = ts.duration;
			context.sm.anim_time = ts.current_time;
			context.sm.anim_time_fraction = ts.current_time / ts.duration;
		}

		bool can_interrupt = (active_transition != nullptr) && active_transition->can_be_interrupted;
		bool is_in_transition = is_blending_previous || has_frozen_pose;

		if (is_in_transition && can_interrupt)
		{
			for (const auto t : current_state->transitions)
			{
				const Transition *resolved_tr = nullptr;
				AnimNodeState *resolved = t->resolveTransition(context, &resolved_tr);
				if (resolved != nullptr)
				{
					pending_interruption = true;
					pending_target = resolved;
					pending_transition = resolved_tr;
					break;
				}
			}
		} else if (is_in_transition == false)
		{
			for (const auto t : current_state->transitions)
			{
				const Transition *resolved_tr = nullptr;
				AnimNodeState *resolved = t->resolveTransition(context, &resolved_tr);
				if (resolved)
				{
					previous_state = current_state;
					is_blending_previous = true;
					current_state = resolved;
					active_transition = resolved_tr;
					transition_time = 0.0f;
					transition_duration = resolved_tr->duration;
					previous_state->onExit(context);
					float enter_t = resolved_tr->computeEnterTime(context);
					current_state->onEnter(context, enter_t);
					current_state_time = 0.0f;
					break;
				}
			}
		}

		// advance transition time and compute progress BEFORE state updates
		// (stale transition_progress would invert accumulated_weight > wrong sync group leader > root motion jump)
		bool is_blending = is_blending_previous || has_frozen_pose;
		if (is_blending)
		{
			transition_time += context.ifps;
			if (transition_duration <= transition_time)
			{
				is_blending_previous = false;
				active_transition = nullptr;
				has_frozen_pose = false;
				transition_progress = 1.0f;
			} else
			{
				if (Consts::EPS < transition_duration)
					transition_progress = saturate(transition_time / transition_duration);
				else
					transition_progress = 1.0f;
			}
		} else
		{
			transition_progress = 1.0f;
		}

		current_state->accumulated_weight = max(current_state->accumulated_weight, accumulated_weight * transition_progress);
		current_state->update(context);

		if (is_blending_previous)
		{
			previous_state->accumulated_weight = max(previous_state->accumulated_weight, accumulated_weight * (1.0f - transition_progress));
			previous_state->update(context);
		}

		context.popSMContext();
	}

	void evaluate(AnimGraphContext &context) override
	{
		if (takeAndClearNeedEvaluate() == false) return;
		if (current_state == nullptr) return;

		current_state->evaluate(context);

		if (has_frozen_pose)
		{
			// blend frozen snapshot with current state
			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->lerpLayer(pose_layer, frozen_pose_layer, current_state->getSafePoseLayer(context), transition_progress);

			// re-snapshot for chained interruption
			if (pending_interruption)
			{
				context.skeleton_pose->copyLayer(frozen_pose_layer, pose_layer);
				if (context.isActiveRootMotion())
					frozen_root_motion_delta = blendTransform(frozen_root_motion_delta, current_state->root_motion_delta, transition_progress);
			}

			if (context.isActiveRootMotion())
				root_motion_delta = blendTransform(frozen_root_motion_delta, current_state->root_motion_delta, transition_progress);
		} else if (is_blending_previous)
		{
			previous_state->evaluate(context);

			pose_layer = context.getTmpPoseLayer();
			context.skeleton_pose->lerpLayer(pose_layer, previous_state->getSafePoseLayer(context), current_state->getSafePoseLayer(context), transition_progress);

			if (context.isActiveRootMotion())
				root_motion_delta = blendTransform(previous_state->root_motion_delta, current_state->root_motion_delta, transition_progress);

			// snapshot for pending interruption
			if (pending_interruption)
			{
				context.skeleton_pose->copyLayer(frozen_pose_layer, pose_layer);
				if (context.isActiveRootMotion())
					frozen_root_motion_delta = root_motion_delta;
			}
		} else
		{
			pose_layer = current_state->getSafePoseLayer(context);

			if (context.isActiveRootMotion())
				root_motion_delta = current_state->root_motion_delta;
		}
	}
};

// Defined here because it needs complete AnimNodeStateMachine type
UNIGINE_INLINE void AnimNodeState::resetPlaybackNodes(AnimNodeBase *node, AnimGraphContext &context)
{
	if (node == nullptr)
		return;
	switch (node->node_type)
	{
		case TYPE::ANIMATION_PLAYER:
		{
			auto *p = static_cast<AnimNodeAnimationPlayer *>(node);
			p->was_inactive = false;
			p->root_motion_delta = transform_identity;
			p->syncToGroup(context, true); // before_update: need_update is stale
			p->teleportTime(p->current_time); // no delta on state entry
			break;
		}
		case TYPE::ANIMATION_PLAYER_SYNC:
		{
			auto *p = static_cast<AnimNodeAnimationPlayerSync *>(node);
			p->teleportTime(0.0f); // no delta on state entry
			p->was_inactive = false;
			p->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::BLEND:
		{
			auto *b = static_cast<AnimNodeBlend *>(node);
			resetPlaybackNodes(b->link_0.node, context);
			resetPlaybackNodes(b->link_1.node, context);
			b->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::BLEND_SPACE_2D:
		{
			auto *bs = static_cast<AnimNodeBlendSpace2D *>(node);
			bs->was_inactive = false;
			bs->snapWeights();
			for (int i = 0; i < bs->links.size(); i++)
				resetPlaybackNodes(bs->links[i].node, context);
			bs->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::BLEND_SPACE_2D_SYNC:
		{
			auto *bs = static_cast<AnimNodeBlendSpace2DSync *>(node);
			bs->was_inactive = false;
			bs->snapWeights();
			bs->syncToGroup(context, true); // before_update: need_update is stale
			for (int i = 0; i < bs->links.size(); i++)
				resetPlaybackNodes(bs->links[i].node, context);
			if (Consts::EPS < bs->normalized_time)
				bs->syncChildrenTime(); // override children time after reset
			bs->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::BRANCH:
		{
			auto *br = static_cast<AnimNodeBranch *>(node);
			resetPlaybackNodes(br->link_true.node, context);
			resetPlaybackNodes(br->link_false.node, context);
			br->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::MAKE_ADDITIVE:
		{
			auto *m = static_cast<AnimNodeMakeAdditive *>(node);
			resetPlaybackNodes(m->link_base_pose.node, context);
			resetPlaybackNodes(m->link_ref_pose.node, context);
			m->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::APPLY_ADDITIVE:
		{
			auto *a = static_cast<AnimNodeApplyAdditive *>(node);
			resetPlaybackNodes(a->link_base_pose.node, context);
			resetPlaybackNodes(a->link_additive.node, context);
			a->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::STATE:
		{
			auto *s = static_cast<AnimNodeState *>(node);
			resetPlaybackNodes(s->link_pose.node, context);
			s->root_motion_delta = transform_identity;
			break;
		}
		case TYPE::STATE_MACHINE:
		{
			auto *sm = static_cast<AnimNodeStateMachine *>(node);
			sm->current_state = nullptr;
			sm->previous_state = nullptr;
			sm->is_blending_previous = false;
			sm->active_transition = nullptr;
			sm->transition_time = 0.0f;
			sm->transition_duration = 0.0f;
			sm->transition_progress = 1.0f;
			sm->current_state_time = 0.0f;
			sm->has_frozen_pose = false;
			sm->frozen_root_motion_delta = transform_identity;
			sm->pending_interruption = false;
			sm->pending_target = nullptr;
			sm->pending_transition = nullptr;
			sm->entry_resolve_failed = false;
			sm->root_motion_delta = transform_identity;
			break;
		}
		default: break;
	}
}

UNIGINE_INLINE void AnimNodeState::applyNormalizedTime(AnimNodeBase *node, AnimGraphContext &context, float normalized_time)
{
	if (node == nullptr)
		return;
	switch (node->node_type)
	{
		case TYPE::ANIMATION_PLAYER:
		{
			auto *p = static_cast<AnimNodeAnimationPlayer *>(node);
			// Sync group has authority over enter_time - skip override when group is active
			if (p->syncker_index != -1 && context.synckers[p->syncker_index].hasOtherActiveMembers(p))
				break;
			if (Consts::EPS < p->duration)
				p->current_time = normalized_time * p->duration;
			if (p->syncker_index != -1)
				context.synckers[p->syncker_index].normalized_time = normalized_time;
			break;
		}
		case TYPE::ANIMATION_PLAYER_SYNC:
		{
			auto *p = static_cast<AnimNodeAnimationPlayerSync *>(node);
			if (Consts::EPS < p->duration)
				p->current_time = normalized_time * p->duration;
			break;
		}
		case TYPE::BLEND:
		{
			auto *b = static_cast<AnimNodeBlend *>(node);
			applyNormalizedTime(b->link_0.node, context, normalized_time);
			applyNormalizedTime(b->link_1.node, context, normalized_time);
			break;
		}
		case TYPE::BLEND_SPACE_2D:
		{
			auto *bs = static_cast<AnimNodeBlendSpace2D *>(node);
			for (int i = 0; i < bs->links.size(); i++)
				applyNormalizedTime(bs->links[i].node, context, normalized_time);
			break;
		}
		case TYPE::BLEND_SPACE_2D_SYNC:
		{
			auto *bs = static_cast<AnimNodeBlendSpace2DSync *>(node);
			// Sync group has authority over enter_time - skip override when group is active
			if (bs->syncker_index != -1 && context.synckers[bs->syncker_index].hasOtherActiveMembers(bs))
				break;
			bs->normalized_time = normalized_time;
			bs->current_time = normalized_time * bs->duration;
			bs->syncChildrenTime();
			if (bs->syncker_index != -1)
				context.synckers[bs->syncker_index].normalized_time = normalized_time;
			break;
		}
		case TYPE::BRANCH:
		{
			auto *br = static_cast<AnimNodeBranch *>(node);
			applyNormalizedTime(br->link_true.node, context, normalized_time);
			applyNormalizedTime(br->link_false.node, context, normalized_time);
			break;
		}
		case TYPE::MAKE_ADDITIVE:
		{
			auto *m = static_cast<AnimNodeMakeAdditive *>(node);
			applyNormalizedTime(m->link_base_pose.node, context, normalized_time);
			break;
		}
		case TYPE::APPLY_ADDITIVE:
		{
			auto *a = static_cast<AnimNodeApplyAdditive *>(node);
			applyNormalizedTime(a->link_base_pose.node, context, normalized_time);
			break;
		}
		case TYPE::STATE:
		{
			auto *s = static_cast<AnimNodeState *>(node);
			applyNormalizedTime(s->link_pose.node, context, normalized_time);
			break;
		}
		default: break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Debug Info
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimNodeDebugInfo
{
	// Execution state
	HashMap<UGUID, float> child_weights;
	int pose_layer{-1};
	float accumulated_weight{0.0f};  // Weight from FinalPose to this node [0..1]

	// Time/Progress state
	float current_time{-1.0f};
	float duration{-1.0f};
	float normalized_time{-1.0f};
	float transition_progress{-1.0f};
	float transition_time{-1.0f};
	float transition_duration{-1.0f};
	float speed{1.0f};
	bool is_loop{false};
	float frame{-1.0f};

	// Weight state
	float blend_weight{-1.0f};

	// Structural state
	UGUID current_state_runtime_guid;
	UGUID previous_state_runtime_guid;
	UGUID active_transition_runtime_guid;

	// BlendSpace state
	vec2 blend_point{vec2_zero};
	Vector<float> sample_weights;

	// Animation
	UGUID asset_guid;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Pose View Info
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct PoseViewInfo
{
	vec4 color{vec4_black};
	float bone_radius{0.0045f};
	bool depth_test{false};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Node Debug Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
struct AnimGraphDebugContext
{
	HashMap<UGUID, AnimNodeDebugInfo> active_nodes;
	HashMap<UGUID, PoseViewInfo> viewed_poses;

	void addViewedPose(const UGUID &node_runtime_guid, const vec4 &color = vec4_black)
	{
		PoseViewInfo info;
		info.color = color;
		viewed_poses[node_runtime_guid] = info;
	}

	void removeViewedPose(const UGUID &node_runtime_guid)
	{
		viewed_poses.erase(node_runtime_guid);
	}

	void setViewedPoseColor(const UGUID &node_runtime_guid, const vec4 &color)
	{
		if (viewed_poses.contains(node_runtime_guid))
			viewed_poses[node_runtime_guid].color = color;
	}

	bool isViewedPose(const UGUID &node_runtime_guid) const
	{
		return viewed_poses.contains(node_runtime_guid);
	}

	void clearViewedPoses()
	{
		viewed_poses.clear();
	}

	// fix_root_in_origin: if true, each preview skeleton will be fixed at origin based on its own root bone position
	// root_joint: index of root bone for fix_root calculation
	void renderViewedPoses(const NodeSkeletonPosePtr &skeleton_pose, bool fix_root_in_origin = false, int root_joint = 0)
	{
		if (skeleton_pose.isNull() || viewed_poses.empty())
			return;

		int num_layers = skeleton_pose->getNumLayers();
		for (const auto &pair : viewed_poses)
		{
			auto iter = active_nodes.find(pair.key);
			if (iter == active_nodes.end())
				continue;

			const AnimNodeDebugInfo &debug_info = iter->data;
			if (debug_info.pose_layer < 0 || num_layers <= debug_info.pose_layer)
				continue;

			// Calculate per-pose fix_root transform
			Mat4 transform = Mat4_identity;
			if (fix_root_in_origin && 0 <= root_joint)
			{
				mat4 joint_tr = skeleton_pose->getLayerJointTransform(debug_info.pose_layer, root_joint);
				Vec3 pos = Vec3(joint_tr.getTranslate());
				transform = translate(Vec3(-pos.x, -pos.y, 0.0));
			}

			const PoseViewInfo &view_info = pair.data;
			skeleton_pose->renderLayerBones(debug_info.pose_layer, transform, view_info.color, view_info.bone_radius, view_info.depth_test);
		}
	}

	static bool isActiveNode(const AnimNodeBase *node)
	{
		return node != nullptr && node->runtime_guid.isValid() && node->need_update == false;
	}

	void collectDebugInfo(const Vector<AnimNodeBase *> &nodes)
	{
		active_nodes.clear();

		for (const auto node : nodes)
		{
			if (node == nullptr || node->runtime_guid.isEmpty())
				continue;

			if (node->need_update == false)
			{
				AnimNodeDebugInfo info;
				info.pose_layer = node->pose_layer;
				info.accumulated_weight = node->accumulated_weight;

				switch (node->node_type)
				{
					case AnimNodeBase::TYPE::FINAL_POSE:
					{
						auto *final_pose = static_cast<AnimNodeFinalPose *>(node);
						if (isActiveNode(final_pose->link.node))
							info.child_weights[final_pose->link.node->runtime_guid] = 1.0f;
						break;
					}
					case AnimNodeBase::TYPE::ANIMATION_POSE:
					{
						auto *anim_pose = static_cast<AnimNodeAnimationPose *>(node);
						info.asset_guid = anim_pose->asset_guid;
						info.frame = anim_pose->frame;
						break;
					}
					case AnimNodeBase::TYPE::ANIMATION_PLAYER:
					{
						auto *player = static_cast<AnimNodeAnimationPlayer *>(node);
						info.current_time = player->current_time;
						info.duration = player->duration;
						info.normalized_time = (Consts::EPS < player->duration) ? (player->current_time / player->duration) : 0.0f;
						info.asset_guid = player->asset_guid;
						info.speed = player->speed;
						info.is_loop = player->is_loop;
						break;
					}
					case AnimNodeBase::TYPE::ANIMATION_PLAYER_SYNC:
					{
						auto *player_sync = static_cast<AnimNodeAnimationPlayerSync *>(node);
						info.current_time = player_sync->current_time;
						info.duration = player_sync->duration;
						info.normalized_time = (Consts::EPS < player_sync->duration) ? (player_sync->current_time / player_sync->duration) : 0.0f;
						info.asset_guid = player_sync->asset_guid;
						info.speed = player_sync->speed;
						break;
					}
					case AnimNodeBase::TYPE::BLEND:
					{
						auto *blend = static_cast<AnimNodeBlend *>(node);
						info.blend_weight = blend->weight;
						if (isActiveNode(blend->link_0.node))
						{
							if (blend->blend_mask == -1)
								info.child_weights[blend->link_0.node->runtime_guid] = 1.0f - blend->weight;
							else
								info.child_weights[blend->link_0.node->runtime_guid] = 1.0f;
						}
						if (isActiveNode(blend->link_1.node))
							info.child_weights[blend->link_1.node->runtime_guid] = blend->weight;
						break;
					}
					case AnimNodeBase::TYPE::BRANCH:
					{
						auto *branch = static_cast<AnimNodeBranch *>(node);
						PoseLink &active = branch->condition ? branch->link_true : branch->link_false;
						if (isActiveNode(active.node))
							info.child_weights[active.node->runtime_guid] = 1.0f;
						break;
					}
					case AnimNodeBase::TYPE::MAKE_ADDITIVE:
					{
						auto *make_add = static_cast<AnimNodeMakeAdditive *>(node);
						if (isActiveNode(make_add->link_base_pose.node))
							info.child_weights[make_add->link_base_pose.node->runtime_guid] = 1.0f;
						if (isActiveNode(make_add->link_ref_pose.node))
							info.child_weights[make_add->link_ref_pose.node->runtime_guid] = 1.0f;
						break;
					}
					case AnimNodeBase::TYPE::APPLY_ADDITIVE:
					{
						auto *apply_add = static_cast<AnimNodeApplyAdditive *>(node);
						info.blend_weight = apply_add->weight;
						if (isActiveNode(apply_add->link_base_pose.node))
							info.child_weights[apply_add->link_base_pose.node->runtime_guid] = 1.0f;
						if (isActiveNode(apply_add->link_additive.node))
							info.child_weights[apply_add->link_additive.node->runtime_guid] = apply_add->weight;
						break;
					}
					case AnimNodeBase::TYPE::BLEND_SPACE_2D:
					{
						auto *bs = static_cast<AnimNodeBlendSpace2D *>(node);
						info.blend_point = bs->blend_point;
						info.sample_weights.clear();
						for (int i = 0; i < bs->smoothed_weights.size(); i++)
							info.sample_weights.append(bs->smoothed_weights[i]);
						for (int i = 0; i < bs->links.size(); i++)
						{
							if (isActiveNode(bs->links[i].node))
								info.child_weights[bs->links[i].node->runtime_guid] = bs->smoothed_weights[i];
						}
						break;
					}
					case AnimNodeBase::TYPE::BLEND_SPACE_2D_SYNC:
					{
						auto *bs = static_cast<AnimNodeBlendSpace2DSync *>(node);
						info.current_time = bs->current_time;
						info.duration = bs->duration;
						info.normalized_time = bs->normalized_time;
						info.speed = bs->speed;
						info.blend_point = bs->blend_point;
						info.sample_weights.clear();
						for (int i = 0; i < bs->smoothed_weights.size(); i++)
							info.sample_weights.append(bs->smoothed_weights[i]);
						for (int i = 0; i < bs->links.size(); i++)
						{
							if (isActiveNode(bs->links[i].node))
								info.child_weights[bs->links[i].node->runtime_guid] = bs->smoothed_weights[i];
						}
						break;
					}
					case AnimNodeBase::TYPE::STATE:
					{
						auto *state = static_cast<AnimNodeState *>(node);
						if (isActiveNode(state->link_pose.node))
							info.child_weights[state->link_pose.node->runtime_guid] = 1.0f;
						break;
					}
					case AnimNodeBase::TYPE::STATE_MACHINE:
					{
						auto *sm = static_cast<AnimNodeStateMachine *>(node);
						info.transition_progress = sm->transition_progress;
						info.transition_time = sm->transition_time;
						info.transition_duration = sm->transition_duration;
						if (sm->current_state != nullptr)
							info.current_state_runtime_guid = sm->current_state->runtime_guid;
						if (sm->previous_state != nullptr)
							info.previous_state_runtime_guid = sm->previous_state->runtime_guid;
						if (sm->active_transition != nullptr)
							info.active_transition_runtime_guid = sm->active_transition->runtime_guid;
						if (isActiveNode(sm->current_state))
							info.child_weights[sm->current_state->runtime_guid] = sm->transition_progress;
						if (isActiveNode(sm->previous_state))
							info.child_weights[sm->previous_state->runtime_guid] = 1.0f - sm->transition_progress;
						break;
					}
					default:
					{
						Log::warning("AnimGraphDebugContext::collectDebugInfo(): unknown node type %d\n", static_cast<int>(node->node_type));
						break;
					}
				}

				active_nodes[node->runtime_guid] = info;
			}
		}

	}

	void clearActiveNodes()
	{
		active_nodes.clear();
	}

	void clear()
	{
		active_nodes.clear();
		viewed_poses.clear();
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Anim Script Base
///////////////////////////////////////////////////////////////////////////////////////////////////////

class AnimScriptBase
{
public:
	AnimScriptBase() = default;
	virtual ~AnimScriptBase() = default;

	// execution sequence
	virtual void init()
	{
		if (skeleton_pose.isNull()) return;
		skeleton_pose->setNumLayers(1);
		skeleton_pose->resetLayerToBindPose(0);
		context.skeleton_pose = skeleton_pose;

		nodes.append(&final_pose);
	}
	virtual void update(float ifps)
	{
		if (skeleton_pose.isNull()) return;
		context.skeleton_pose = skeleton_pose;
		context.ifps = ifps;

		context.self_timed_players.clear();

		for (auto n : nodes)
		{
			if (n->need_update)
			{
				if (n->node_type == AnimNodeBase::TYPE::ANIMATION_PLAYER || n->node_type == AnimNodeBase::TYPE::ANIMATION_PLAYER_SYNC)
					static_cast<AnimNodeAnimationPlayerBase *>(n)->was_inactive = true;
				else if (n->node_type == AnimNodeBase::TYPE::BLEND_SPACE_2D_SYNC)
					static_cast<AnimNodeBlendSpace2DSync *>(n)->was_inactive = true;
				else if (n->node_type == AnimNodeBase::TYPE::BLEND_SPACE_2D)
					static_cast<AnimNodeBlendSpace2D *>(n)->was_inactive = true;
			}

			n->need_update = true;
			n->need_evaluate = true;
			n->accumulated_weight = 0.0f;
		}
	}
	virtual void shutdown()
	{
		if (skeleton_pose.isValid())
			skeleton_pose->setNumLayers(1);
	}

	// parameters
	enum PARAM_TYPE
	{
		PARAM_TYPE_UNKNOWN = 0,
		PARAM_TYPE_INT,
		PARAM_TYPE_FLOAT,
		PARAM_TYPE_BOOL,
		PARAM_TYPE_QUAT,
		PARAM_TYPE_VEC2,
		PARAM_TYPE_VEC3,
		PARAM_TYPE_VEC4,
		PARAM_TYPE_IVEC2,
		PARAM_TYPE_IVEC3,
		PARAM_TYPE_IVEC4,
		PARAM_TYPE_MAT3,
		PARAM_TYPE_MAT4,
		PARAM_TYPE_ANIM_ASSET,
		PARAM_TYPE_TRIGGER,
		NUM_PARAM_TYPES,
	};

	virtual int getNumParams() const { return 0; }
	virtual int getParamType(int /*index*/) const { return PARAM_TYPE_UNKNOWN; }
	virtual const char *getParamName(int /*index*/) const { return "<wrong_param_name>"; }

	virtual void resetParam(const char * /*name*/) {}

	virtual void setParamInt(const char * /*name*/, int /*value*/) {}
	virtual int getParamInt(const char * /*name*/) const { return 0; }

	virtual void setParamFloat(const char * /*name*/, float /*value*/) {}
	virtual float getParamFloat(const char * /*name*/) const { return 0.0f; }

	virtual void setParamBool(const char * /*name*/, bool /*value*/) {}
	virtual bool getParamBool(const char * /*name*/) const { return false; }

	virtual void setParamQuat(const char * /*name*/, const quat & /*value*/) {}
	virtual const quat &getParamQuat(const char * /*name*/) const { return quat_identity; }

	virtual void setParamVec2(const char * /*name*/, const vec2 & /*value*/) {}
	virtual const vec2 &getParamVec2(const char * /*name*/) const { return vec2_zero; }

	virtual void setParamVec3(const char * /*name*/, const vec3 & /*value*/) {}
	virtual const vec3 &getParamVec3(const char * /*name*/) const { return vec3_zero; }

	virtual void setParamVec4(const char * /*name*/, const vec4 & /*value*/) {}
	virtual const vec4 &getParamVec4(const char * /*name*/) const { return vec4_zero; }

	virtual void setParamIVec2(const char * /*name*/, const ivec2 & /*value*/) {}
	virtual const ivec2 &getParamIVec2(const char * /*name*/) const { return ivec2_zero; }

	virtual void setParamIVec3(const char * /*name*/, const ivec3 & /*value*/) {}
	virtual const ivec3 &getParamIVec3(const char * /*name*/) const { return ivec3_zero; }

	virtual void setParamIVec4(const char * /*name*/, const ivec4 & /*value*/) {}
	virtual const ivec4 &getParamIVec4(const char * /*name*/) const { return ivec4_zero; }

	virtual void setParamMat3(const char * /*name*/, const mat3 & /*value*/) {}
	virtual const mat3 &getParamMat3(const char * /*name*/) const { return mat3_identity; }

	virtual void setParamMat4(const char * /*name*/, const mat4 & /*value*/) {}
	virtual const mat4 &getParamMat4(const char * /*name*/) const { return mat4_identity; }

	virtual void setParamAnimAsset(const char * /*name*/, const UGUID & /*guid*/) {}
	virtual const UGUID &getParamAnimAsset(const char * /*name*/) const { return UGUID::empty; }

	virtual void setParamTrigger(const char * /*name*/, bool /*value*/) {}
	virtual bool getParamTrigger(const char * /*name*/) const { return false; }

	void initPoseLayers()
	{
		for (auto n : nodes)
			n->grabConstPoseLayer(context);
	}

	void updateFinalPose()
	{
		// Release tmp pose layers from previous frame BEFORE creating new ones
		// This keeps layers alive until next frame so editor can render previews
		context.releaseTmpPoseLayers();

		final_pose.update(context);

		for (auto &syncker : context.synckers)
			syncker.update(context);

		for (auto player : context.self_timed_players)
			player->nextTime(context);

		final_pose.evaluate(context);

		if (debug_collecting)
			debug_context.collectDebugInfo(nodes);
		// renderViewedPoses called by editor with correct mesh transform
	}

	void setNodeSkeletonPose(const NodeSkeletonPosePtr &node) { skeleton_pose = node; }

	void setDebugCollecting(bool enabled) { debug_collecting = enabled; }
	bool isDebugCollecting() const { return debug_collecting; }

	AnimGraphDebugContext *getDebugContext() { return &debug_context; }
	const AnimGraphDebugContext *getDebugContext() const { return &debug_context; }

	bool isActiveRootMotion() const { return context.isActiveRootMotion(); }
	const Transform &getRootMotionDelta() const { return context.root_motion_delta; }

	bool isStateMachineActive(const char *sm_name) const
	{
		const auto *sm = find_state_machine(sm_name);
		return sm && (sm->need_update == false);
	}

	const char *getStateMachineCurrentStateName(const char *sm_name) const
	{
		const auto *sm = find_state_machine(sm_name);
		if (sm && sm->current_state)
			return sm->current_state->state_name.get();
		return "";
	}

	const char *getStateMachinePreviousStateName(const char *sm_name) const
	{
		const auto *sm = find_state_machine(sm_name);
		if (sm && sm->previous_state)
			return sm->previous_state->state_name.get();
		return "";
	}

	bool isStateMachineInTransition(const char *sm_name) const
	{
		const auto *sm = find_state_machine(sm_name);
		if (sm == nullptr)
			return false;
		return sm->is_blending_previous || sm->has_frozen_pose;
	}

	float getStateMachineTransitionProgress(const char *sm_name) const
	{
		const auto *sm = find_state_machine(sm_name);
		if (sm == nullptr)
			return 1.0f;
		return sm->transition_progress;
	}

protected:
	const AnimNodeStateMachine *find_state_machine(const char *sm_name) const
	{
		for (const auto *n : nodes)
		{
			if (n->node_type == AnimNodeBase::TYPE::STATE_MACHINE && static_cast<const AnimNodeStateMachine *>(n)->sm_name == sm_name)
				return static_cast<const AnimNodeStateMachine *>(n);
		}
		return nullptr;
	}


	NodeSkeletonPosePtr skeleton_pose;
	AnimGraphContext context;
	AnimGraphDebugContext debug_context;

	AnimNodeFinalPose final_pose;
	Vector<AnimNodeBase *> nodes;

	bool anim_assets_changed{false};
	bool debug_collecting{false};
};

}

}
