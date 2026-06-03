// ThirdPersonAnimationHelper
// Управляет анимацией персонажа через Anim Graph (NodeSkeletonPose + AnimScript).
//
// Параметры графа (.agraph):
//   state (Int)  — дискретный выбор состояния в State Machine графа:
//       0 = IDLE, 1 = WALK, 2 = RUN, 3 = JUMP, 4 = CROUCH, 5 = INTERACT
//   blend (Float) — 0..1 continuous blend внутри одного состояния
//       (например speed 0..1 для Walk→Run, или crouchAmount)
//
// Логика:
//   state устанавливается мгновенно по CharacterState.
//   blend плавно интерполируется к NormalizedSpeed или к 1 для прыжка.
//   При INTERACT blend = 1 (один раз), затем возвращается к предыдущему.

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
	[Parameter(Group = "Anim Graph", Tooltip = "Имя Int-параметра в Anim Graph для выбора состояния")]
	private string stateParamName = "state";

	[ShowInEditor]
	[Parameter(Group = "Anim Graph", Tooltip = "Имя Float-параметра в Anim Graph для blend (walk→run, crouch amount, etc.)")]
	private string blendParamName = "blend";

	[ShowInEditor]
	[ParameterSlider(Min = 0.1f, Max = 20.0f, Group = "Anim Graph",
		Tooltip = "Скорость интерполяции blend: больше = быстрее")]
	private float blendSpeed = 5.0f;

	[ShowInEditor]
	[Parameter(Group = "Debug", Tooltip = "Логировать смену состояний")]
	private bool verbose = false;

	#endregion

	#region Private State

	private AnimScript animScript = null;
	private bool failed = false;

	// Текущий сглаженный blend
	private float currentBlend = 0.0f;

	// Защита от повторного выставления state (храним предыдущее)
	private int previousState = -1;

	// При INTERACT надо вернуться на предыдущий state после одного кадра
	private bool interactPending = false;

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

		// --- Инициализировать параметры ---
		currentBlend = 0.0f;
		previousState = -1;
		animScript.SetParamInt(stateParamName, 0);
		animScript.SetParamFloat(blendParamName, 0.0f);
		skeletonPose.Play();

		// Подписка на смену состояния
		movementController.OnAnimationStateChanged += OnStateChanged;

		Log.Message($"[ThirdPersonAnimationHelper] OK. Граф: \"{animScript.FilePath}\", state=\"{stateParamName}\", blend=\"{blendParamName}\"\n");
	}

	private void Update()
	{
		if (failed || movementController == null || animScript == null)
			return;

		if (!animScript.IsInit)
			return;

		// --- State (дискретный, устанавливается мгновенно при смене) ---
		int targetState = (int)movementController.State;

		// Если INTERACT — выставляем мгновенно, но на следующем кадре возвращаем
		if (targetState == (int)ThirdPersonMovementController.CharacterState.INTERACT)
		{
			if (!interactPending)
			{
				interactPending = true;
				if (verbose)
					Log.Message("[ThirdPersonAnimationHelper] INTERACT triggered\n");
			}
		}
		else if (interactPending)
		{
			interactPending = false;
		}

		if (targetState != previousState)
		{
			animScript.SetParamInt(stateParamName, targetState);
			previousState = targetState;
			if (verbose)
				Log.Message($"[ThirdPersonAnimationHelper] state → {targetState} ({movementController.State})\n");
		}

		// --- Blend (плавная интерполяция) ---
		float targetBlend = ComputeTargetBlend();
		currentBlend = MathLib.Lerp(currentBlend, targetBlend, blendSpeed * Game.IFps);
		currentBlend = MathLib.Clamp(currentBlend, 0.0f, 1.0f);
		animScript.SetParamFloat(blendParamName, currentBlend);

		// Обновить позу каждый кадр — обязательно для MODE_ANIM_SCRIPT
		skeletonPose.UpdatePose(Game.IFps);
	}

	private void Shutdown()
	{
		if (movementController != null)
		{
			movementController.OnAnimationStateChanged -= OnStateChanged;
		}
		animScript = null;
		skeletonPose = null;
		movementController = null;
	}

	#region Blend Logic

	/// <summary>
	/// Вычисляет целевой blend (0..1) для текущего состояния:
	///   IDLE/CROUCH → 0 (или NormalizedSpeed для CROUCH для постепенности)
	///   WALK → NormalizedSpeed (0..1, обычно ~0.5)
	///   RUN → NormalizedSpeed (0..1, обычно ~1.0)
	///   JUMP → 1.0 (максимум — фаза взлёта)
	///   INTERACT → 1.0
	/// </summary>
	private float ComputeTargetBlend()
	{
		switch (movementController.State)
		{
			case ThirdPersonMovementController.CharacterState.IDLE:
				return 0.0f;

			case ThirdPersonMovementController.CharacterState.WALK:
			case ThirdPersonMovementController.CharacterState.RUN:
				// NormalizedSpeed от контроллера: 0 = стоя, 1 = runSpeed
				// Для WALK будет ~0.5, для RUN ~1.0 — граф сам решит что показывать
				return movementController.NormalizedSpeed;

			case ThirdPersonMovementController.CharacterState.JUMP:
				return 1.0f;

			case ThirdPersonMovementController.CharacterState.CROUCH:
				// Crouch: blend от скорости передвижения (0..1 от crouchSpeed)
				return movementController.NormalizedSpeed;

			case ThirdPersonMovementController.CharacterState.INTERACT:
				return 1.0f;

			default:
				return 0.0f;
		}
	}

	#endregion

	#region Callbacks

	private void OnStateChanged(ThirdPersonMovementController.CharacterState oldState, ThirdPersonMovementController.CharacterState newState)
	{
		if (failed || animScript == null)
			return;

		// Если соединение разорвано, не дёргаем
		// State устанавливается в Update; здесь только лог
		if (verbose)
		{
			Log.Message($"[ThirdPersonAnimationHelper] State: {oldState} → {newState} (int: {(int)newState})\n");
		}
	}

	#endregion

	#region Helpers

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

	/// <summary>Текущее значение blend (0..1).</summary>
	public float CurrentBlend => currentBlend;

	/// <summary>True, если компонент не смог инициализироваться.</summary>
	public bool Failed => failed;

	#endregion
}