// Minimal test component: plays a single idle animation via Anim Graph.
// Assign to a node that has a NodeSkeletonPose child or assign SkeletonPose directly.
// The .agraph has no parameters — just Animation Player → Output Pose with idle.anim.

#region Math Variables
#if UNIGINE_DOUBLE
using Scalar = System.Double;
using Vec2 = Unigine.dvec2;
using Vec3 = Unigine.dvec3;
using Vec4 = Unigine.dvec4;
using Mat4 = Unigine.dmat4;
#else
using Scalar = System.Single;
using Vec2 = Unigine.vec2;
using Vec3 = Unigine.vec3;
using Vec4 = Unigine.vec4;
using Mat4 = Unigine.mat4;
using WorldBoundBox = Unigine.BoundBox;
using WorldBoundSphere = Unigine.BoundSphere;
using WorldBoundFrustum = Unigine.BoundFrustum;
#endif
#endregion

using Unigine;

public partial class SimpleIdlePlayer : Component
{
	[ShowInEditor]
	[Parameter(Tooltip = "NodeSkeletonPose with idle_only.agraph assigned (auto-search if empty)")]
	private NodeSkeletonPose skeletonPose = null;

	private AnimScript animScript = null;

	private void Init()
	{
		// Auto-find NodeSkeletonPose
		if (skeletonPose == null)
		{
			skeletonPose = node as NodeSkeletonPose;
			if (skeletonPose == null)
				skeletonPose = GetComponent<NodeSkeletonPose>(node);
			if (skeletonPose == null)
				skeletonPose = FindNodeSkeletonPoseRecursive(node);
		}

		if (skeletonPose == null)
		{
			Log.Error("SimpleIdlePlayer: No NodeSkeletonPose found on node or its children.\n");
			return;
		}

		// Assign the idle-only anim graph
		skeletonPose.Mode = NodeSkeletonPose.MODE.ANIM_SCRIPT;
		skeletonPose.AnimScriptPath = "third_person_controller/malchisch/animations/idle_only.agraph";

		// Get AnimScript and verify it initialized
		animScript = skeletonPose.GetAnimScript();
		if (animScript == null || !animScript.IsInit)
		{
			Log.Error("SimpleIdlePlayer: AnimScript not initialized!\n");
			return;
		}

		// Start playback
		skeletonPose.Play();

		Log.Message("SimpleIdlePlayer: initialized OK — playing idle.anim via idle_only.agraph\n");
	}

	private void Update()
	{
		if (skeletonPose == null || animScript == null)
			return;

		// Update the pose every frame — required for MODE_ANIM_SCRIPT
		skeletonPose.UpdatePose(Game.IFps);
	}

	private static NodeSkeletonPose FindNodeSkeletonPoseRecursive(Node parent)
	{
		int count = parent.NumChildren;
		for (int i = 0; i < count; i++)
		{
			Node child = parent.GetChild(i);
			NodeSkeletonPose nsp = child as NodeSkeletonPose;
			if (nsp != null)
				return nsp;
			nsp = FindNodeSkeletonPoseRecursive(child);
			if (nsp != null)
				return nsp;
		}
		return null;
	}
}