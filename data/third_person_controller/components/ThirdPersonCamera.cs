// Simple third-person follow camera.
// Orbits around a target node using spherical coordinates.
// Supports manual orbit (mouse), collision, and zoom.
// Smooth follow: exponential damping on position + slerp on look-at.

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

public partial class ThirdPersonCamera : Component
{
	[ShowInEditor]
	[Parameter(Group = "Target", Tooltip = "Node to follow")]
	private Node target = null;

	[ShowInEditor]
	[ParameterSlider(Min = 10.0f, Max = 120.0f, Group = "Camera")]
	private float fov = 60.0f;

	[ShowInEditor]
	[ParameterSlider(Min = 0.01f, Group = "Camera")]
	private float nearClipping = 0.1f;

	[ShowInEditor]
	[ParameterSlider(Min = 10.0f, Group = "Camera")]
	private float farClipping = 1000.0f;

	[ShowInEditor]
	[ParameterSlider(Min = 0.5f, Group = "Orbit")]
	private float distance = 5.0f;

	[ShowInEditor]
	[ParameterSlider(Min = -89.9f, Max = 89.9f, Group = "Orbit", Tooltip = "Vertical angle in degrees (negative = camera above target)")]
	private float verticalAngle = -25.0f;

	[ShowInEditor]
	[ParameterSlider(Min = 0.0f, Group = "Orbit")]
	private float angularSpeed = 120.0f;

	[ShowInEditor]
	[Parameter(Group = "Orbit")]
	private bool invertY = false;

	// --- Smoothing ---
	[ShowInEditor]
	[ParameterSlider(Min = 0.5f, Max = 20.0f, Group = "Smooth",
		Tooltip = "Position follow speed. Higher = snappier, lower = more cinematic lag. LN-style: ~4-6")]
	private float positionFollowSpeed = 5.0f;

	[ShowInEditor]
	[ParameterSlider(Min = 0.5f, Max = 20.0f, Group = "Smooth",
		Tooltip = "Look-at rotation speed. Higher = snappier. LN-style: ~8-10")]
	private float lookFollowSpeed = 8.0f;

	// --- Collision ---
	[ShowInEditor]
	[Parameter(Group = "Collision")]
	private bool useCollision = true;

	[ShowInEditor]
	[ParameterSlider(Min = 0.0f, Group = "Collision")]
	private float collisionRadius = 0.3f;

	[ShowInEditor]
	[ParameterMask(Group = "Collision", MaskType = ParameterMaskAttribute.TYPE.INTERSECTION)]
	private int collisionMask = 1;

	// --- Input ---
	[ShowInEditor]
	[Parameter(Group = "Input", Tooltip = "Enable/disable camera rotation via mouse")]
	private bool cameraRotationEnabled = false;

	// --- Private state ---
	private PlayerDummy camera = null;
	private float horizontalAngle = 0.0f;
	private bool initialized = false;

	// Smoothed pivot point (the point camera orbits around)
	private Vec3 smoothedPivot = Vec3.ZERO;

	private WorldIntersectionNormal rayHit = new WorldIntersectionNormal();

	// --- Public API ---
	public PlayerDummy Camera => camera;
	public float HorizontalAngle => horizontalAngle;
	public float VerticalAngle => verticalAngle;

	public bool CameraRotationEnabled
	{
		get => cameraRotationEnabled;
		set => cameraRotationEnabled = value;
	}

	public void SetHorizontalAngle(float angleDeg)
	{
		horizontalAngle = angleDeg;
	}

	// --- Lifecycle ---
	private void Init()
	{
		if (target == null)
		{
			Log.Error("ThirdPersonCamera: Target is not assigned.\n");
			return;
		}

		camera = new PlayerDummy();
		camera.Fov = fov;
		camera.ZNear = nearClipping;
		camera.ZFar = farClipping;
		camera.MainPlayer = true;
		Game.Player = camera;

		// Инициализируем сглаженный пивот сразу на персонажа
		smoothedPivot = target.WorldPosition;

		// Ставим камеру мгновенно — без сглаживания при старте
		Vec3 initPos = ComputeDesiredPos(smoothedPivot);
		camera.WorldPosition = initPos;
		SetLookAt(initPos, smoothedPivot, 1.0f);

		initialized = true;
		Log.Message("ThirdPersonCamera: initialized.\n");
	}

	private void Update()
	{
		if (!initialized || camera == null || target == null)
			return;

		float ifps = Game.IFps;

		// --- Ручное вращение мышью ---
		if (cameraRotationEnabled)
		{
			ivec2 mouseDelta = Input.MouseDeltaPosition;
			horizontalAngle += mouseDelta.x * angularSpeed * ifps;
			float vDelta = mouseDelta.y * angularSpeed * ifps;
			if (invertY) vDelta = -vDelta;
			verticalAngle += vDelta;
			verticalAngle = MathLib.Clamp(verticalAngle, -80.0f, 80.0f);
		}

		UpdateCameraTransform(ifps);
	}

	private void UpdateCameraTransform(float ifps)
	{
		Vec3 rawTargetPos = target.WorldPosition;

		// --- Плавное следование за персонажем (exponential smoothing) ---
		// Используем exp-decay: frame-rate независимое сглаживание
		// Little Nightmares: камера "тянется" за персонажем с лёгкой инерцией
		float alpha = 1.0f - MathLib.Exp(-positionFollowSpeed * ifps);
		smoothedPivot = smoothedPivot + (rawTargetPos - smoothedPivot) * alpha;

		// --- Желаемая позиция камеры (вокруг сглаженного пивота) ---
		Vec3 desiredPos = ComputeDesiredPos(smoothedPivot);

		// --- Коллизия: проверяем от реальной позиции персонажа ---
		if (useCollision)
			desiredPos = ResolveCollision(rawTargetPos, desiredPos);

		// При коллизии — мгновенно прижимаем (иначе камера проходит сквозь стену)
		// При нормальном движении — тоже мгновенно (позиция и так сглажена через pivot)
		camera.WorldPosition = desiredPos;

		// --- Плавный look-at (slerp по кватернионам) ---
		// Смотрим на реального персонажа, не на сглаженный пивот
		SetLookAt(desiredPos, rawTargetPos, lookFollowSpeed * ifps);
	}

	// Вычислить желаемую позицию камеры вокруг заданной точки
	private Vec3 ComputeDesiredPos(Vec3 pivot)
	{
		float hRad = horizontalAngle * MathLib.DEG2RAD;
		float vRad = verticalAngle * MathLib.DEG2RAD;

		vec3 dir = new vec3(
			MathLib.Cos(vRad) * MathLib.Sin(hRad),
			MathLib.Cos(vRad) * MathLib.Cos(hRad),
			-MathLib.Sin(vRad)
		);
		dir.Normalize();

		return pivot - new Vec3(dir) * distance;
	}

	// Плавно повернуть камеру на цель
	private void SetLookAt(Vec3 camPos, Vec3 lookAtPos, float t)
	{
		Vec3 lookDir = lookAtPos - camPos;
		if (lookDir.Length2 < MathLib.EPSILON)
			return;

		lookDir.Normalize();
		vec3 targetDirF = new vec3(lookDir);

		// Защита от gimbal lock (взгляд почти прямо вверх/вниз)
		vec3 upVec = vec3.UP;
		if (MathLib.Abs(MathLib.Dot(targetDirF, upVec)) > 0.99f)
			upVec = new vec3(0.0f, 1.0f, 0.0f);

		t = MathLib.Clamp(t, 0.0f, 1.0f);

		if (t >= 1.0f)
		{
			camera.SetWorldDirection(targetDirF, upVec, MathLib.AXIS.NZ);
		}
		else
		{
			// Slerp кватернионов — нет дёрганий на больших углах
			quat fromQ = new quat(camera.GetWorldRotation());
			mat4 toMat = MathLib.SetTo(vec3.ZERO, targetDirF, upVec, MathLib.AXIS.NZ);
			quat toQ = new quat(new mat3(toMat));
			quat resultQ = MathLib.Slerp(fromQ, toQ, t);
			// Извлекаем направление -Z (forward для NZ камеры)
			vec3 smoothDir = resultQ * new vec3(0.0f, 0.0f, -1.0f);
			camera.SetWorldDirection(smoothDir, upVec, MathLib.AXIS.NZ);
		}
	}

	// Коллизия камеры со средой
	private Vec3 ResolveCollision(Vec3 targetPos, Vec3 desiredPos)
	{
		Vec3 toCamera = desiredPos - targetPos;
		float dist = toCamera.Length;
		if (dist < MathLib.EPSILON)
			return desiredPos;

		Vec3 dir = toCamera / dist;

		Object hit = World.GetIntersection(new vec3(targetPos), new vec3(desiredPos), collisionMask, rayHit);
		if (hit != null)
		{
			float hitDist = (new Vec3(rayHit.Point) - targetPos).Length;
			float safe = MathLib.Max(hitDist - collisionRadius, 0.1f);
			return targetPos + dir * safe;
		}

		return desiredPos;
	}
}