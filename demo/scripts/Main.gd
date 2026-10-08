extends Node3D

@onready var _subscriber: ZmqSubscriber = $ZmqSubscriber
@onready var _video: TextureRect = $CanvasLayer/VideoView
@onready var _mesh: MeshInstance3D = $MeshInstance3D


func _ready() -> void:
	_subscriber.message_received.connect(_on_message_received)
	_subscriber.error_occurred.connect(_on_error_occurred)


func _process(delta: float) -> void:
	_mesh.rotate_y(delta)


func _on_message_received(_topic: String, payload: PackedByteArray) -> void:
	_video.push_jpeg(payload)


func _on_error_occurred(message: String) -> void:
	push_warning('ZmqSubscriber: ' + message)
