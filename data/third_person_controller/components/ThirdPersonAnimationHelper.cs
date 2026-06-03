// ThirdPersonAnimationHelper
// Управляет анимацией персонажа через Anim Graph (NodeSkeletonPose + AnimScript).
//
// Структура графа animation_graph.agraph:
//   [Animation Player: idle]  → A ─┐
//                                   ├─ [Blend Poses] → [Output Pose]
//   [Animation Player: jog_forward] → B ─┘
//   [Float: weight 0..1] ──────────→ Weight
//
// Логика:
//   weight = 0.0  → чистый Idle
//   weight = 1.0  → чистый Run (jog_forward)
//   Переход плавный: weight движется к цели с configurable скоростью.

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

// Компонент-мост между ThirdPersonMovementController и Anim Graph.
// Назначьте на корневую ноду персонажа (ту же, что содержит NodeSkeletonPose в иерархии).
public partial class ThirdPersonAnimationHelper : Component
{
	#region Editor Parameters

	[ShowInEditor]
	[Parameter(Group = "References", Tooltip = "ThirdPersonMovementController (авто-поиск если пусто)")]
	private ThirdPersonMovementController movementController = null;

	[ShowInEditor]
	[Parameter(Group = "References", Tooltip = "NodeSkeletonPose с назначенным .agraph (авто-поиск если пусто)")]
	private NodeSkeletonPose skeletonPose = null;

	[ShowInEditor]
	[Parameter(Group = "Anim Graph", Tooltip = "Имя Float-параметра в Anim Graph (см. Float-нода в графе)")]
	private string weightParamName = "weight";

	[ShowInEditor]
	[ParameterSlider(Min = 0.1f, Max = 20.0f, Group = "Anim Graph",
		Tooltip = "Скорость интерполяции weight: больше = быстрее смена анимации")]
	private float blendSpeed = 5.0f;

	#endregion

	#region Private State

	private AnimScript animScript = null;
	private float currentWeight = 0.0f;   // текущее значение weight (Idle=0, Run=1)
	private bool failed = false;

	#endregion

	private void Init()
	{
		// --- Найти MovementController ---
		if (movementController == null)
			movementController = GetComponent<ThirdPersonMovementController>(node);

		if (movementController == null)
		{
			Log.Error("[ThirdPersonAnimationHelper] ThirdPersonMovementController не найден!\n");
			failed = true;
			return;
		}

		// --- Найти NodeSkeletonPose ---
		if (skeletonPose == null)
			skeletonPose = node as NodeSkeletonPose;
		if (skeletonPose == null)
			skeletonPose = GetComponent<NodeSkeletonPose>(node);
		if (skeletonPose == null)
			skeletonPose = FindNodeSkeletonPoseRecursive(node);

		if (skeletonPose == null)
		{
			Log.Error("[ThirdPersonAnimationHelper] NodeSkeletonPose не найден в иерархии!\n");
			failed = true;
			return;
		}

		// --- Получить AnimScript ---
		animScript = skeletonPose.GetAnimScript();
		if (animScript == null)
		{
			Log.Error("[ThirdPersonAnimationHelper] AnimScript == null. Убедитесь, что в NodeSkeletonPose назначен .agraph и Mode = ANIM_SCRIPT.\n");
			failed = true;
			return;
		}

		if (!animScript.IsInit)
		{
			Log.Error("[ThirdPersonAnimationHelper] AnimScript не инициализирован (неверный путь к .agraph?).\n");
			failed = true;
			return;
		}

		// --- Стартовать проигрывание ---
		currentWeight = 0.0f;
		animScript.SetParamFloat(weightParamName, currentWeight);
		skeletonPose.Play();

		Log.Message($"[ThirdPersonAnimationHelper] OK. Граф: \"{animScript.FilePath}\", параметр: \"{weightParamName}\"\n");
	}

	private void Update()
	{
		if (failed || movementController == null || animScript == null)
			return;

		// Целевое значение weight:
		//   IDLE → 0.0
		//   RUN  → 1.0
		float targetWeight = IsRunning() ? 1.0f : 0.0f;

		// Плавная интерполяция к цели
		currentWeight = MathLib.Lerp(currentWeight, targetWeight, blendSpeed * Game.IFps);
		currentWeight = MathLib.Clamp(currentWeight, 0.0f, 1.0f);

		// Передать параметр в граф
		animScript.SetParamFloat(weightParamName, currentWeight);

		// Обновить позу каждый кадр — обязательно для MODE_ANIM_SCRIPT
		skeletonPose.UpdatePose(Game.IFps);
	}

	private void Shutdown()
	{
		animScript = null;
		skeletonPose = null;
		movementController = null;
	}

	#region Helpers

	// Проверяет, движется ли персонаж
	private bool IsRunning()
	{
		return movementController.State == ThirdPersonMovementController.CharacterState.RUN
			|| movementController.State == ThirdPersonMovementController.CharacterState.WALK;
	}

	// Рекурсивный поиск NodeSkeletonPose в дочерних нодах
	private static NodeSkeletonPose FindNodeSkeletonPoseRecursive(Node parent)
	{
		for (int i = 0; i < parent.NumChildren; i++)
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

	#endregion

	#region Public API

	/// <summary>AnimScript из NodeSkeletonPose.</summary>
	public AnimScript AnimScript => animScript;

	/// <summary>NodeSkeletonPose персонажа.</summary>
	public NodeSkeletonPose SkeletonPose => skeletonPose;

	/// <summary>Текущее значение weight (0 = idle, 1 = run).</summary>
	public float CurrentWeight => currentWeight;

	/// <summary>True, если компонент не смог инициализироваться.</summary>
	public bool Failed => failed;

	#endregion
}