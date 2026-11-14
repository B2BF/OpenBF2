# [TEMP] [CHOSEN, not recovered] Play-test fixture, not game code: loopback.py --pose / --scenario /
# --mark-session copies this module into the cloned server root (mods/bf2/python/game/) and imports it
# from game/__init__.py. It uses only the retail server Python API (host.registerHandler for
# PlayerConnect / PlayerSpawn / PlayerKilled / ChatMessage, bf2.Timer, soldier.getPosition / setPosition /
# getRotation / setRotation / getDamage), so the same file works on the retail dedicated server, run
# from a copied server root (never inside the original game directory).
#
# Scenario file (written next to this module by loopback.py): playtest_pose.cfg. '#' starts a comment.
#   player X Y Z [YAW [PITCH [ROLL]]]        every spawn of a human player is moved here
#   pose NAME X Y Z [YAW]                    a named spot: a human writing "goto NAME" in chat is moved there
#   bot friend|enemy|team1|team2 X Y Z [YAW] the first AI player of that team to spawn (the next one
#                                            for each further line of the same team) is moved here,
#                                            on every spawn of that bot and on every human spawn
# friend / enemy are relative to the team of the first human player who spawns. The legacy one-line
# form "X Y Z [YAW [PITCH [ROLL]]]" equals a single player line. An empty or missing file only logs.
# The client owns its view direction: the rotation does not turn a human's view (scripts use aim).
#
# Chat: a human writing "mark NAME" (any channel) logs that player's position, rotation and map as a
# [mark] line and appends ready-to-use scenario / script lines to playtest_marks.txt; "goto NAME" moves
# the writer to the named pose (scripts use it to visit several spots in one run).
#
# Log: playtest_pose.log next to this module (the embedded interpreter does not route print to the
# server console). Spawns, placements, kills, placed-bot health changes and marks are logged.

import host
import math
import bf2

# The retail server's embedded Python 2.3 is stripped: its os module has no os.path (the import of
# this module stopped with "AttributeError: 'module' object has no attribute 'path'" in a Debug
# assertion dialog), and re is not relied on either. Paths and the chat keywords are plain string
# operations; __file__ is absolute on the macOS harness and relative to the server root on retail.
def _dirOf(path):
	cut = max(path.rfind('/'), path.rfind('\\'))
	if cut < 0:
		return ''
	return path[:cut + 1]


_here = _dirOf(__file__)


def _path(name):
	return _here + name

_player = None          # ((x, y, z), (yaw, pitch, roll)) or None
_poses = {}             # NAME -> ((x, y, z), (yaw, 0, 0))
_slots = []             # {'side', 'pos', 'rot', 'index' (assigned AI player), 'last' (logged state)}
_humanTeam = None
_spawnOrder = {1: [], 2: []}   # AI player indices per team, in first-spawn order
_timer = None


def _log(text):
	try:
		f = open(_path('playtest_pose.log'), 'a')
		f.write(text + '\n')
		f.close()
	except Exception:
		pass


def _fmt(v):
	return '%.1f/%.1f/%.1f' % (v[0], v[1], v[2])


def _rot3(values):
	values = list(values)
	while len(values) < 3:
		values.append(0.0)
	return tuple(values[0:3])


def _load():
	global _player
	try:
		f = open(_path('playtest_pose.cfg'), 'r')
		lines = f.read().split('\n')
		f.close()
	except Exception, e:
		_log('[pose] no scenario file (%s); logging only' % e)
		return
	for raw in lines:
		words = raw.split('#')[0].split()
		if not words:
			continue
		try:
			if words[0] == 'player' or words[0][0] in '-+.0123456789':
				if words[0] == 'player':
					words = words[1:]
				values = [float(w) for w in words]
				if len(values) < 3:
					raise ValueError('needs x y z')
				_player = (tuple(values[0:3]), _rot3(values[3:6]))
			elif words[0] == 'pose':
				values = [float(w) for w in words[2:]]
				if len(values) < 3:
					raise ValueError('needs x y z')
				_poses[words[1]] = (tuple(values[0:3]), _rot3(values[3:4]))
			elif words[0] == 'bot':
				side = words[1]
				if side not in ('friend', 'enemy', 'team1', 'team2'):
					raise ValueError('bot side must be friend, enemy, team1 or team2')
				values = [float(w) for w in words[2:]]
				if len(values) < 3:
					raise ValueError('needs x y z')
				_slots.append({'side': side, 'pos': tuple(values[0:3]), 'rot': _rot3(values[3:4]),
							   'index': None, 'last': None})
			else:
				raise ValueError('unknown keyword')
		except Exception, e:
			_log('[pose] scenario line ignored (%s): %s' % (e, raw.strip()))
	if _player:
		_log('[pose] player pose %s rotation %s' % (_fmt(_player[0]), _fmt(_player[1])))
	for name in _poses.keys():
		_log('[pose] named pose %s at %s yaw %.1f' % (name, _fmt(_poses[name][0]), _poses[name][1][0]))
	for slot in _slots:
		_log('[pose] bot slot %s at %s yaw %.1f' % (slot['side'], _fmt(slot['pos']), slot['rot'][0]))
	if not _player and not _slots and not _poses:
		_log('[pose] empty scenario; logging spawns only')


def _slotTeam(slot):
	side = slot['side']
	if side == 'team1':
		return 1
	if side == 'team2':
		return 2
	if _humanTeam is None:
		return None
	if side == 'friend':
		return _humanTeam
	return 3 - _humanTeam


def _assign():
	# The k-th slot of a team takes the k-th AI player of that team to spawn; an assignment is kept.
	taken = {}
	for slot in _slots:
		if slot['index'] is not None:
			taken[slot['index']] = 1
	for slot in _slots:
		if slot['index'] is not None:
			continue
		team = _slotTeam(slot)
		if team is None:
			continue
		for index in _spawnOrder.get(team, []):
			if not taken.has_key(index):
				slot['index'] = index
				taken[index] = 1
				_log('[pose] bot slot %s (team %d) -> player %d %s' % (
					slot['side'], team, index, host.pmgr_p_get('name', index)))
				break


def _placeSlot(slot, soldier=None):
	index = slot['index']
	if index is None:
		return
	if soldier is None:
		if not host.pmgr_p_get('alive', index):
			return      # (during its own PlayerSpawn event a bot is not alive yet; the soldier is passed)
		# The wrappers are new objects on every call, so compare template names, not objects.
		soldier = host.pmgr_p_get('defaultvehicle', index)
		vehicle = host.pmgr_p_get('vehicle', index)
		if soldier is None or vehicle is None or vehicle.templateName != soldier.templateName:
			_log('[pose] bot %s %s not on foot; left in place' % (slot['side'], host.pmgr_p_get('name', index)))
			return
	before = soldier.getPosition()
	soldier.setPosition(slot['pos'])
	soldier.setRotation(slot['rot'])
	_log('[pose] bot %s %s at %s -> %s (now %s)' % (slot['side'], host.pmgr_p_get('name', index),
		_fmt(before), _fmt(slot['pos']), _fmt(soldier.getPosition())))


def onPlayerSpawn(player, soldier):
	global _humanTeam
	try:
		name = player.getName()
		team = player.getTeam()
		before = soldier.getPosition()
		_log('[pose] PlayerSpawn %d %s team %d ai %d at %s' % (player.index, name, team, player.isAIPlayer() and 1 or 0, _fmt(before)))
		if player.isAIPlayer():
			order = _spawnOrder.setdefault(team, [])
			if player.index not in order:
				order.append(player.index)
			_assign()
			for slot in _slots:
				if slot['index'] == player.index:
					_placeSlot(slot, soldier)
					return
			return
		if _humanTeam is None:
			_humanTeam = team
			_log('[pose] human team %d' % team)
		if _player is None:
			_log('[pose] spawn %s team %d at %s rot %s' % (name, team, _fmt(before), _fmt(soldier.getRotation())))
		else:
			spawnRot = soldier.getRotation()
			soldier.setPosition(_player[0])
			soldier.setRotation(_player[1])
			_log('[pose] spawn %s at %s -> %s rot %s (now %s) spawnrot %s' % (
				name, _fmt(before), _fmt(_player[0]), _fmt(_player[1]), _fmt(soldier.getPosition()), _fmt(spawnRot)))
		_assign()
		for slot in _slots:
			_placeSlot(slot)
	except Exception, e:
		_log('[pose] PlayerSpawn failed: %s' % e)


def onPlayerConnect(player):
	# Logged so a client driver without its own log (the dinput8 proxy on a retail client) can wait on it.
	try:
		_log('[pose] PlayerConnect %d %s' % (player.index, player.getName()))
	except Exception, e:
		_log('[pose] PlayerConnect failed: %s' % e)


def onPlayerKilled(victim, attacker, weapon, assists, obj):
	try:
		vname = victim and victim.getName() or '?'
		aname = attacker and attacker.getName() or '-'
		wname = '-'
		if weapon:
			try:
				wname = weapon.templateName
			except Exception:
				wname = str(weapon)
		_log('[kill] %s (team %s) killed by %s with %s' % (vname, victim and victim.getTeam(), aname, wname))
	except Exception, e:
		_log('[pose] PlayerKilled failed: %s' % e)


def _tick(data=None):
	# Placed bots: log alive / health / position changes (hit confirmation for scripted fire).
	try:
		for slot in _slots:
			index = slot['index']
			if index is None:
				continue
			alive = host.pmgr_p_get('alive', index)
			state = (alive, None, None)
			if alive:
				soldier = host.pmgr_p_get('vehicle', index)
				if soldier is not None:
					p = soldier.getPosition()
					state = (alive, round(soldier.getDamage(), 1), (int(round(p[0])), int(round(p[1])), int(round(p[2]))))
			if state != slot['last']:
				slot['last'] = state
				if alive and state[1] is not None:
					_log('[bot] %s %s alive health %.1f pos %d/%d/%d' % (slot['side'],
						host.pmgr_p_get('name', index), state[1], state[2][0], state[2][1], state[2][2]))
				else:
					_log('[bot] %s %s alive=%d' % (slot['side'], host.pmgr_p_get('name', index), alive and 1 or 0))
	except Exception, e:
		_log('[pose] timer failed: %s' % e)


# The text may carry channel / dead markers in front of the message; search for the keyword.
_wordChars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_'
_nameChars = _wordChars + '.-'


def _keyword(text, word):
	# The NAME after the first "word" that starts the text or follows a non-word character and is
	# followed by whitespace, or None (what the regex (?:^|[^A-Za-z0-9_])word\s+([A-Za-z0-9_.-]+) found).
	start = 0
	while 1:
		at = text.find(word, start)
		if at < 0:
			return None
		start = at + 1
		if at > 0 and text[at - 1] in _wordChars:
			continue
		i = at + len(word)
		j = i
		while j < len(text) and text[j] in ' \t\r\n':
			j = j + 1
		if j == i:
			continue
		k = j
		while k < len(text) and text[k] in _nameChars:
			k = k + 1
		if k > j:
			return text[j:k]


def _goto(playerId, label):
	pname = host.pmgr_p_get('name', playerId)
	if not _poses.has_key(label):
		_log('[goto] %s by %s: no such pose' % (label, pname))
		return
	soldier = host.pmgr_p_get('vehicle', playerId)
	if soldier is None or not host.pmgr_p_get('alive', playerId):
		_log('[goto] %s by %s: no living soldier' % (label, pname))
		return
	pos, rot = _poses[label]
	before = soldier.getPosition()
	soldier.setPosition(pos)
	soldier.setRotation(rot)
	_log('[goto] %s %s at %s -> %s (now %s)' % (label, pname, _fmt(before), _fmt(pos), _fmt(soldier.getPosition())))


def onChatMessage(playerId, text, channel, flags):
	try:
		if playerId < 0:
			return
		name = _keyword(text, 'goto')
		if name:
			_goto(playerId, name)
			return
		label = _keyword(text, 'mark')
		if not label:
			return
		pname = host.pmgr_p_get('name', playerId)
		team = host.pmgr_p_get('team', playerId)
		obj = host.pmgr_p_get('vehicle', playerId)
		if obj is None or not host.pmgr_p_get('alive', playerId):
			_log('[mark] %s by %s: no living soldier' % (label, pname))
			return
		try:
			obj = bf2.objectManager.getRootParent(obj)
		except Exception:
			pass
		p = obj.getPosition()
		r = obj.getRotation()
		mapName = host.sgl_getMapName()
		yaw = math.radians(r[0])
		aim = (p[0] + 15.0 * math.sin(yaw), p[1] + 1.2, p[2] + 15.0 * math.cos(yaw))
		_log('[mark] %s pos %.2f/%.2f/%.2f rot %.1f/%.1f/%.1f map %s team %d by %s; aim %.1f/%.1f/%.1f' % (
			label, p[0], p[1], p[2], r[0], r[1], r[2], mapName, team, pname, aim[0], aim[1], aim[2]))
		f = open(_path('playtest_marks.txt'), 'a')
		f.write('# mark %s map %s team %d by %s (%s)\n' % (label, mapName, team, pname, channel))
		f.write('player %.2f %.2f %.2f %.1f    # or: bot enemy|friend %.2f %.2f %.2f %.1f\n' % (
			p[0], p[1], p[2], r[0], p[0], p[1], p[2], r[0]))
		f.write('aim %.1f/%.1f/%.1f 1.0 5000    # 15 m ahead along the yaw, chest height\n' % aim)
		f.write('--pose %.2f/%.2f/%.2f/%.1f\n\n' % (p[0], p[1], p[2], r[0]))
		f.close()
		try:
			host.sgl_sendTextMessage(playerId, 10, 1, 'mark %s: %.1f/%.1f/%.1f yaw %.0f' % (label, p[0], p[1], p[2], r[0]))
		except Exception:
			pass
	except Exception, e:
		_log('[mark] ChatMessage failed: %s' % e)


_load()
host.registerHandler('PlayerSpawn', onPlayerSpawn, 1)
host.registerHandler('PlayerConnect', onPlayerConnect, 1)
host.registerHandler('PlayerKilled', onPlayerKilled)
host.registerHandler('ChatMessage', onChatMessage, 1)
if _slots:
	try:
		_timer = bf2.Timer(_tick, 1, 1)
		_timer.setRecurring(1)
	except Exception, e:
		_log('[pose] no bot timer: %s' % e)
_log('[pose] handlers registered (PlayerConnect, PlayerSpawn, PlayerKilled, ChatMessage)')
