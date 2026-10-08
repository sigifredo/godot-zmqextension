extends TextureRect

var _image := Image.new()
var _texture: ImageTexture


func push_jpeg(bytes: PackedByteArray) -> void:
	var error := _image.load_jpg_from_buffer(bytes)
	if error != OK:
		push_warning('VideoView: JPEG inválido (%s)' % error_string(error))
		return
	if _texture != null and _texture.get_size() == Vector2(_image.get_size()):
		_texture.update(_image)
	else:
		_texture = ImageTexture.create_from_image(_image)
		texture = _texture
