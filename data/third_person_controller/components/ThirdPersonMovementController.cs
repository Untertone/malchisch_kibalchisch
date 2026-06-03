// Third-person character controller for Little Nightmares-style gameplay.
// Simplified version: direct transform manipulation, no physics body.
// Features: 8-directional movement relative to camera, jump, ground check via raycast.

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

using System.Collections.Generic;
using Unigine;

public partial class ThirdPersonMovementController : Component
{
	#region Animation State Enum
	public enum CharacterState
	{
		IDLE, WALK, RUN, JUMP_UP, JUMP_FALL, LAND,
		CROUCH, CRAWL, LEDGE_GRAB_IDLE, LEDGE_GRAB_MOVE, CLIMB_UP,
		SWING, PUSH, PULL, INTERACT, CLIMB_LADDER
	}
	#endregion

	#region Editor parameters

	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Forward key")]
	private Input.KEY forwardKey = Input.KEY.W;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Backward key")]
	private Input.KEY backwardKey = Input.KEY.S;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Right key")]
	private Input.KEY rightKey = Input.KEY.D;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Left key")]
	private Input.KEY leftKey = Input.KEY.A;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Jump key")]
	private Input.KEY jumpKey = Input.KEY.SPACE;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Interact key")]
	private Input.KEY interactKey = Input.KEY.E;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Crouch/Crawl key")]
	private Input.KEY crouchKey = Input.KEY.ANY_CTRL;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Toggle camera rotation key")]
	private Input.KEY toggleCameraRotationKey = Input.KEY.M;
	[ShowInEditor][Parameter(Group = "Input", Tooltip = "Run modifier key")]
	private Input.KEY runKey = Input.KEY.ANY_SHIFT;

	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Walk speed (units/sec)")]
	private float walkSpeed = 3.0f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Run speed (units/sec)")]
	private float runSpeed = 6.0f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Acceleration")]
	private float acceleration = 20.0f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Deceleration / damping")]
	private float damping = 15.0f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Jump power")]
	private float jumpPower = 8.0f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Movement", Tooltip = "Gravity multiplier")]
	private float gravityMultiplier = 2.5f;
	[ShowInEditor][ParameterSlider(Min = 0.0f, Max = 90.0f, Group = "Movement", Tooltip = "Max ground angle (degrees)")]
	private float maxGroundAngle = 45.0f;

	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Body", Tooltip = "Ground check distance")]
	private float groundCheckDistance = 0.2f;
	[ShowInEditor][ParameterMask(Group = "Body", MaskType = ParameterMaskAttribute.TYPE.INTERSECTION, Tooltip = "Ground check mask")]
	private int groundCheckMask = 1;

	[ShowInEditor][Parameter(Group = "Camera", Tooltip = "Reference to ThirdPersonCamera component")]
	private ThirdPersonCamera cameraComponent = null;

	[ShowInEditor][ParameterSlider(Min = 0.0f, Group = "Interaction", Tooltip = "Max interaction distance")]
	private float interactDistance = 2.0f;
	[ShowInEditor][ParameterMask(Group = "Interaction", MaskType = ParameterMaskAttribute.TYPE.INTERSECTION, Tooltip = "Interaction raycast mask")]
	private int interactMask = 1;

	#endregion

	#region Public properties

	public bool IsInitialized { get; private set; } = false;
	public CharacterState State { get; private set; } = CharacterState.IDLE;
	public bool IsGround { get; private set; } = false;
	public Vec3 Velocity { get; private set; } = Vec3.ZERO;
	public float Speed { get; private set; } = 0.0f;
	public Vec3 MoveDirection { get; private set; } = Vec3.FORWARD;

	public delegate void AnimationStateChanged(CharacterState oldState, CharacterState newState);
	public event AnimationStateChanged OnAnimationStateChanged = null;

	#endregion

	#region Private state

	private Vec2 inputDirection = Vec2.ZERO;
	private float verticalVelocity = 0.0f;
	private Vec3 currentVelocity = Vec3.ZERO;
	private Vec3 slopeNormal = Vec3.UP;
	private float gravityAccel = 0.0f;
	private float cosGroundAngle = 0.707f;

	private WorldIntersectionNormal rayHit = new WorldIntersectionNormal();

	private bool interactRequested = false;
	private bool jumpRequested = false;
	private bool toggleCameraRotationRequested = false;
	private bool previousToggleCameraRotationState = false;

	#endregion

	#region Lifecycle

	private void Init()
	{
		Object obj = node as Object;
		if (!obj)
		{
			Log.Error("ThirdPersonMovementController: node must be an Object\n");
			return;
		}

		Input.MouseHandle = Input.MOUSE_HANDLE.GRAB;

		gravityAccel = Physics.Gravity.z * gravityMultiplier;
		cosGroundAngle = MathLib.Cos(maxGroundAngle * MathLib.DEG2RAD);

		// Reset transform: axis Y = forward, UP = Z
		node.SetWorldDirection(new vec3(0.0f, 1.0f, 0.0f), vec3.UP, MathLib.AXIS.Y);

		IsInitialized = true;
		Log.Message("ThirdPersonMovementController initialized.\n");
	}

	private void Update()
	{
		if (!IsInitialized)
			return;

		float ifps = Game.IFps;

		UpdateInput();
		UpdateMovement(ifps);
		// Сначала поворот — чтобы SetWorldDirection не сдвинул позицию,
		// если у ноды есть локальное смещение пивота
		UpdateCharacterRotation(ifps);
		UpdateTransform();
	}

	#endregion

	#region Input

	private void UpdateInput()
	{
		inputDirection = Vec2.ZERO;
		jumpRequested = false;
		interactRequested = false;
		toggleCameraRotationRequested = false;

		if (!Input.MouseGrab)
			return;

		if (Input.IsKeyPressed(forwardKey))  inputDirection.y += 1.0f;
		if (Input.IsKeyPressed(backwardKey)) inputDirection.y -= 1.0f;
		if (Input.IsKeyPressed(rightKey))    inputDirection.x += 1.0f;
		if (Input.IsKeyPressed(leftKey))     inputDirection.x -= 1.0f;

		if (inputDirection.Length2 > 0.01f)
			inputDirection.Normalize();

		jumpRequested = Input.IsKeyDown(jumpKey);
		interactRequested = Input.IsKeyDown(interactKey);

		if (interactRequested)
			TryInteract();

		// Toggle camera rotation on key press (edge detection)
		bool currentToggleState = Input.IsKeyPressed(toggleCameraRotationKey);
		if (currentToggleState && !previousToggleCameraRotationState)
		{
			toggleCameraRotationRequested = true;
			ToggleCameraRotation();
		}
		previousToggleCameraRotationState = currentToggleState;
	}

	#endregion

	#region Movement

	private void UpdateMovement(float ifps)
	{
		CheckGround();

		// Gravity
		if (!IsGround)
			verticalVelocity += gravityAccel * ifps;
		else if (verticalVelocity < 0.0f)
			verticalVelocity = 0.0f;

		// Jump
		if (jumpRequested && IsGround)
		{
			verticalVelocity = jumpPower;
			IsGround = false;
		}

		// Horizontal movement
		Vec3 targetVelocity = Vec3.ZERO;
		if (inputDirection.Length2 > 0.01f)
		{
			Vec3 camForward, camRight;
			GetCameraRelativeDirections(out camForward, out camRight);

			Vec3 wishDir = camForward * inputDirection.y + camRight * inputDirection.x;
			if (wishDir.Length2 > MathLib.EPSILON)
			{
				wishDir.Normalize();

				if (IsGround)
				{
					float proj = MathLib.Dot(wishDir, slopeNormal);
					wishDir = (wishDir - slopeNormal * proj).Normalized;
				}

				MoveDirection = wishDir;

				float speed = Input.IsKeyPressed(runKey) ? runSpeed : walkSpeed;
				targetVelocity = wishDir * speed;
			}
		}

		Vec3 horizontalVel = new Vec3(currentVelocity.x, currentVelocity.y, 0.0f);
		Vec3 targetHorizontal = new Vec3(targetVelocity.x, targetVelocity.y, 0.0f);

		if (targetHorizontal.Length2 > 0.01f)
		{
			// Exponential acceleration — frame-rate independent, без дёргания на старте
			float accelAlpha = 1.0f - MathLib.Exp(-acceleration * ifps);
			horizontalVel = horizontalVel + (targetHorizontal - horizontalVel) * accelAlpha;
			if (horizontalVel.Length > targetVelocity.Length)
				horizontalVel = horizontalVel.Normalized * targetVelocity.Length;
		}
		else
		{
			// Exponential deceleration
			float speed = horizontalVel.Length;
			if (speed > 0.001f)
			{
				float decelAlpha = 1.0f - MathLib.Exp(-damping * ifps);
				speed = speed * (1.0f - decelAlpha);
				if (speed < 0.01f)
					horizontalVel = Vec3.ZERO;
				else
					horizontalVel = horizontalVel / horizontalVel.Length * speed;
			}
			else
				horizontalVel = Vec3.ZERO;
		}

		currentVelocity.x = horizontalVel.x;
		currentVelocity.y = horizontalVel.y;
		currentVelocity.z = verticalVelocity;

		Velocity = currentVelocity;
		Speed = new Vec3(horizontalVel.x, horizontalVel.y, 0.0f).Length;

		UpdateAnimationState();
	}

	private void UpdateAnimationState()
	{
		CharacterState newState = State;

		if (!IsGround)
			newState = verticalVelocity > 0.0f ? CharacterState.JUMP_UP : CharacterState.JUMP_FALL;
		else if (inputDirection.Length2 > 0.01f)
			newState = Input.IsKeyPressed(runKey) ? CharacterState.RUN : CharacterState.WALK;
		else
			newState = CharacterState.IDLE;

		if (newState != State)
		{
			CharacterState oldState = State;
			State = newState;
			OnAnimationStateChanged?.Invoke(oldState, newState);
		}
	}

	#endregion

	#region Camera-Relative Directions

	private void GetCameraRelativeDirections(out Vec3 forward, out Vec3 right)
	{
		if (cameraComponent != null && cameraComponent.Camera != null)
		{
			vec3 camForwardRaw = cameraComponent.Camera.GetWorldDirection(MathLib.AXIS.NZ);
			forward = new Vec3(camForwardRaw.x, camForwardRaw.y, 0.0f);
			if (forward.Length2 > MathLib.EPSILON)
				forward.Normalize();
			else
				forward = Vec3.FORWARD;

			right = MathLib.Cross(forward, Vec3.UP);
			if (right.Length2 > MathLib.EPSILON)
				right.Normalize();
			else
				right = Vec3.RIGHT;
		}
		else
		{
			forward = Vec3.FORWARD;
			right = Vec3.RIGHT;
		}
	}

	#endregion

	#region Ground Check

	// Позиция земли под персонажем (для snap)
	private Vec3 groundSnapPos = Vec3.ZERO;
	private bool hasGroundSnap = false;

	private void CheckGround()
	{
		slopeNormal = Vec3.UP;
		hasGroundSnap = false;

		// Начинаем raycast чуть выше пивота — чтобы не промахнуться при движении вниз
		Vec3 origin = node.WorldPosition + Vec3.UP * 0.05f;
		float checkDist = groundCheckDistance + 0.05f + 0.05f; // +0.05 offset + запас

		Object hit = World.GetIntersection(new vec3(origin), new vec3(origin - Vec3.UP * checkDist), groundCheckMask, rayHit);
		if (hit != null)
		{
			slopeNormal = new Vec3(rayHit.Normal);
			float dot = MathLib.Dot(slopeNormal, Vec3.UP);
			if (dot >= cosGroundAngle)
			{
				IsGround = true;
				groundSnapPos = new Vec3(rayHit.Point);
				hasGroundSnap = true;
			}
			else
			{
				IsGround = false;
			}
		}
		else
		{
			IsGround = false;
		}
	}

	#endregion

	#region Interaction

	private void TryInteract()
	{
		if (cameraComponent == null || cameraComponent.Camera == null)
			return;

		Vec3 camPos = cameraComponent.Camera.WorldPosition;
		vec3 camDirRaw = cameraComponent.Camera.GetWorldDirection(MathLib.AXIS.NZ);
		Vec3 camDir = new Vec3(camDirRaw);
		Vec3 endPos = camPos + camDir * interactDistance;

		Object hit = World.GetIntersection(new vec3(camPos), new vec3(endPos), interactMask, rayHit);
		if (hit != null)
		{
			Log.Message("ThirdPersonController: interact with {0}\n", hit.Name);
			var oldState = State;
			State = CharacterState.INTERACT;
			OnAnimationStateChanged?.Invoke(oldState, State);
		}
	}

	#endregion

	#region Transform & Rotation

	private void UpdateTransform()
	{
		float ifps = Game.IFps;
		Vec3 moveDelta = currentVelocity * ifps;
		Vec3 curPos = node.WorldPosition;
		Vec3 newPos = curPos + moveDelta;

		if (IsGround && hasGroundSnap && currentVelocity.z <= 0.0f)
		{
			// Плавный snap к земле — без микродребезга Z
			// Если Z-скорость нулевая, прижимаем с exponential smoothing
			float groundZ = groundSnapPos.z;
			float diffZ = groundZ - newPos.z;
			if (MathLib.Abs(diffZ) > 0.001f)
			{
				// Быстрое притяжение к земле (frame-rate independent)
				float snapAlpha = 1.0f - MathLib.Exp(-30.0f * ifps);
				newPos.z += diffZ * snapAlpha;
				if (MathLib.Abs(groundZ - newPos.z) < 0.001f)
					newPos.z = groundZ;
			}
			else
			{
				newPos.z = groundZ;
			}
		}

		node.WorldPosition = newPos;
	}

	// Минимальная скорость поворота (градусов/сек) — для малых углов (диагональ ~45°)
	private const float ROTATION_SPEED_MIN = 180.0f;
	// Максимальная скорость поворота (градусов/сек) — для больших углов (разворот 180°)
	private const float ROTATION_SPEED_MAX = 720.0f;
	// Угол (градусов), при котором скорость = MAX (выше этого порога — всегда MAX)
	private const float ROTATION_FAST_THRESHOLD_DEG = 90.0f;

	private void UpdateCharacterRotation(float ifps)
	{
		if (inputDirection.Length2 < 0.01f)
			return;

		Vec3 faceDir = MoveDirection;
		if (faceDir.Length2 < MathLib.EPSILON)
			return;

		faceDir.z = 0.0f;
		faceDir.Normalize();

		// Текущее направление персонажа (по оси Y в мировых координатах)
		vec3 currentDir = node.GetWorldDirection(MathLib.AXIS.Y);
		vec3 currentDirFlat = new vec3(currentDir.x, currentDir.y, 0.0f);
		if (currentDirFlat.Length2 < MathLib.EPSILON)
		{
			node.SetWorldDirection(new vec3(faceDir), vec3.UP, MathLib.AXIS.Y);
			return;
		}
		currentDirFlat = currentDirFlat.Normalized;

		// Угол между текущим и целевым направлением
		float dot = MathLib.Clamp(MathLib.Dot(currentDirFlat, new vec3(faceDir)), -1.0f, 1.0f);
		float angleDeg = MathLib.Acos(dot) * MathLib.RAD2DEG;

		if (angleDeg < 0.1f)
		{
			node.SetWorldDirection(new vec3(faceDir), vec3.UP, MathLib.AXIS.Y);
			return;
		}

		// Адаптивная скорость: плавно нарастает от MIN (малые углы) до MAX (большие)
		// angleDeg in [0, ROTATION_FAST_THRESHOLD_DEG] → скорость in [MIN, MAX]
		float speedFactor = MathLib.Clamp(angleDeg / ROTATION_FAST_THRESHOLD_DEG, 0.0f, 1.0f);
		float rotationSpeed = MathLib.Lerp(ROTATION_SPEED_MIN, ROTATION_SPEED_MAX, speedFactor);

		float maxStep = rotationSpeed * ifps;

		if (maxStep >= angleDeg)
		{
			// Достигаем цели за один кадр
			node.SetWorldDirection(new vec3(faceDir), vec3.UP, MathLib.AXIS.Y);
		}
		else
		{
			// Поворачиваемся на maxStep градусов через slerp кватернионов
			float t = maxStep / angleDeg;
			quat from = new quat(node.GetWorldRotation());
			mat4 toMat = MathLib.SetTo(vec3.ZERO, new vec3(faceDir), vec3.UP, MathLib.AXIS.Y);
			quat to = new quat(new mat3(toMat));
			quat result = MathLib.Slerp(from, to, t);
			vec3 interpolatedY = result * new vec3(0.0f, 1.0f, 0.0f);
			node.SetWorldDirection(interpolatedY, vec3.UP, MathLib.AXIS.Y);
		}
	}

	#endregion

	#region Camera Rotation Toggle

	private void ToggleCameraRotation()
	{
		if (cameraComponent != null)
		{
			cameraComponent.CameraRotationEnabled = !cameraComponent.CameraRotationEnabled;
			Log.Message("ThirdPersonController: Camera rotation {0}\n",
				cameraComponent.CameraRotationEnabled ? "ENABLED" : "DISABLED");
		}
	}

	/// <summary>Is camera rotation currently enabled?</summary>
	public bool IsCameraRotationEnabled
	{
		get
		{
			return cameraComponent != null ? cameraComponent.CameraRotationEnabled : false;
		}
	}

	#endregion

	#region Public API

	public void SetState(CharacterState newState)
	{
		CharacterState oldState = State;
		State = newState;
		OnAnimationStateChanged?.Invoke(oldState, newState);
	}

	public Vec3 CharacterForward
	{
		get
		{
			vec3 axisY = node.GetWorldDirection(MathLib.AXIS.Y);
			return new Vec3(axisY);
		}
	}

	#endregion
}