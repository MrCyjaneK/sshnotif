package main

// PAM helper invoked by pam_exec from sshd (already root).
// Never fail the login: every path exits 0. Do not start the Silica app.

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const (
	deviceUID      = 100000
	smallFileMax   = 8 << 10
	logRotateBytes = 512 << 10
	iconPath       = "/usr/share/icons/hicolor/86x86/apps/sshnotif.png"
	remoteAction   = "org.sshnotif /org/sshnotif org.sshnotif show"
)

type event struct {
	ID       string `json:"id"`
	TS       int64  `json:"ts"`
	TSISO    string `json:"ts_iso"`
	Type     string `json:"type"`
	User     string `json:"user"`
	Rhost    string `json:"rhost"`
	TTY      string `json:"tty"`
	PID      int    `json:"pid"`
	Duration *int64 `json:"duration_s,omitempty"`
}

type owner struct {
	home string
	uid  int
	gid  int
}

func main() {
	defer func() {
		recover()
		os.Exit(0)
	}()
	_ = run()
}

func run() error {
	evType, ok := pamEventType(os.Getenv("PAM_TYPE"))
	if !ok {
		return nil
	}
	own, err := resolveOwner()
	if err != nil {
		return nil
	}

	now := time.Now()
	ts := now.Unix()
	ppid := os.Getppid()
	pamType := os.Getenv("PAM_TYPE")
	pamUser := os.Getenv("PAM_USER")
	pamRhost := os.Getenv("PAM_RHOST")
	pamTTY := os.Getenv("PAM_TTY")

	dataDir := filepath.Join(own.home, ".local/share/sshnotif")
	logPath := filepath.Join(dataDir, "events.jsonl")
	snoozePath := filepath.Join(dataDir, "snooze-until")
	runDir := "/run/sshnotif"
	lastPath := filepath.Join(runDir, "last")
	sessPath := filepath.Join(runDir, fmt.Sprintf("sess.%d", ppid))

	_ = os.MkdirAll(runDir, 0755)
	_ = mkdirOwner(dataDir, own)

	dupKey := fmt.Sprintf("%s|%s|%s|%d", pamType, pamUser, pamRhost, ppid)
	if isDup(lastPath, dupKey, ts) {
		return nil
	}
	_ = writeFile(lastPath, fmt.Sprintf("%s %d\n", dupKey, ts))

	id := ""
	var duration *int64
	switch evType {
	case "open":
		id = fmt.Sprintf("%d-%d", ppid, ts)
		_ = writeFile(sessPath, fmt.Sprintf("%s %d\n", id, ts))
	case "close":
		id, duration = sessionClose(sessPath, ts)
		if id == "" {
			id = fmt.Sprintf("%d-%d", ppid, ts)
		}
	default:
		id = fmt.Sprintf("%d-%d-a", ppid, ts)
	}

	ev := event{
		ID:       id,
		TS:       ts,
		TSISO:    now.Format(time.RFC3339),
		Type:     evType,
		User:     pamUser,
		Rhost:    pamRhost,
		TTY:      pamTTY,
		PID:      ppid,
		Duration: duration,
	}
	if line, err := jsonLine(ev); err == nil {
		rotateIfNeeded(logPath)
		_ = appendFile(logPath, line)
		_ = os.Chown(logPath, own.uid, own.gid)
		_ = os.Chmod(logPath, 0644)
		_ = os.Chmod(dataDir, 0755)
	}

	if snoozed(snoozePath, ts) {
		return nil
	}
	summary, body := notifyText(evType, pamUser, pamRhost, pamTTY, ev.TSISO, duration)
	notify(own.uid, summary, body)
	return nil
}

func pamEventType(pamType string) (string, bool) {
	switch pamType {
	case "auth":
		return "attempt", true
	case "open_session":
		return "open", true
	case "close_session":
		return "close", true
	default:
		return "", false
	}
}

func resolveOwner() (owner, error) {
	if o, err := ownerFromUID(deviceUID); err == nil {
		return o, nil
	}
	ents, err := os.ReadDir("/home")
	if err != nil {
		return owner{}, err
	}
	best := owner{}
	bestUID := int(^uint(0) >> 1)
	found := false
	for _, e := range ents {
		if strings.HasPrefix(e.Name(), ".") {
			continue
		}
		st, err := os.Stat(filepath.Join("/home", e.Name()))
		if err != nil || !st.IsDir() {
			continue
		}
		sys, ok := st.Sys().(*syscall.Stat_t)
		if !ok {
			continue
		}
		uid := int(sys.Uid)
		if uid < deviceUID || uid >= bestUID {
			continue
		}
		bestUID = uid
		best = owner{home: filepath.Join("/home", e.Name()), uid: uid, gid: int(sys.Gid)}
		found = true
	}
	if !found {
		return owner{}, fmt.Errorf("no device owner")
	}
	return best, nil
}

func ownerFromUID(uid int) (owner, error) {
	u, err := user.LookupId(strconv.Itoa(uid))
	if err != nil || u.HomeDir == "" || !strings.HasPrefix(u.HomeDir, "/") {
		return owner{}, fmt.Errorf("no passwd entry")
	}
	gid, _ := strconv.Atoi(u.Gid)
	return owner{home: u.HomeDir, uid: uid, gid: gid}, nil
}

func mkdirOwner(path string, o owner) error {
	if err := os.MkdirAll(path, 0755); err != nil {
		return err
	}
	return os.Chown(path, o.uid, o.gid)
}

func readSmall(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	b, err := io.ReadAll(io.LimitReader(f, smallFileMax))
	if err != nil {
		return "", err
	}
	return string(bytes.TrimSpace(b)), nil
}

func writeFile(path, data string) error {
	return os.WriteFile(path, []byte(data), 0644)
}

func appendFile(path, data string) error {
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_APPEND|syscall.O_NOFOLLOW, 0644)
	if err != nil {
		return err
	}
	_, err = io.WriteString(f, data)
	if cerr := f.Close(); err == nil {
		err = cerr
	}
	return err
}

func rotateIfNeeded(logPath string) {
	st, err := os.Stat(logPath)
	if err != nil || st.Size() < logRotateBytes {
		return
	}
	_ = os.Rename(logPath, logPath+".prev")
}

func isDup(lastPath, key string, now int64) bool {
	s, err := readSmall(lastPath)
	if err != nil {
		return false
	}
	i := strings.LastIndexByte(s, ' ')
	if i <= 0 {
		return false
	}
	if s[:i] != key {
		return false
	}
	lastTS, err := strconv.ParseInt(strings.TrimSpace(s[i+1:]), 10, 64)
	if err != nil {
		return false
	}
	dt := now - lastTS
	return dt >= 0 && dt <= 1
}

func sessionClose(path string, now int64) (string, *int64) {
	s, err := readSmall(path)
	_ = os.Remove(path)
	if err != nil {
		return "", nil
	}
	id, rest, ok := strings.Cut(s, " ")
	if !ok || id == "" {
		return "", nil
	}
	start, err := strconv.ParseInt(strings.TrimSpace(rest), 10, 64)
	if err != nil {
		return id, nil
	}
	d := now - start
	if d < 0 {
		d = 0
	}
	return id, &d
}

func snoozed(path string, now int64) bool {
	s, err := readSmall(path)
	if err != nil {
		return false
	}
	until, err := strconv.ParseInt(strings.TrimSpace(s), 10, 64)
	if err != nil {
		return false
	}
	return until > now
}

func jsonLine(ev event) (string, error) {
	var buf bytes.Buffer
	enc := json.NewEncoder(&buf)
	enc.SetEscapeHTML(false)
	if err := enc.Encode(ev); err != nil {
		return "", err
	}
	return buf.String(), nil
}

func durationText(secs int64) string {
	if secs < 0 {
		secs = 0
	}
	h := secs / 3600
	m := (secs % 3600) / 60
	s := secs % 60
	switch {
	case h > 0:
		return fmt.Sprintf("%dh %dm %ds", h, m, s)
	case m > 0:
		return fmt.Sprintf("%dm %ds", m, s)
	default:
		return fmt.Sprintf("%ds", s)
	}
}

func orUnknown(s string) string {
	if s == "" {
		return "unknown"
	}
	return s
}

func notifyText(evType, user, rhost, tty, iso string, duration *int64) (string, string) {
	u, h := orUnknown(user), orUnknown(rhost)
	td := tty
	if td == "" {
		td = "none"
	}
	switch evType {
	case "attempt":
		return fmt.Sprintf("SSH attempt: %s from %s", u, h),
			fmt.Sprintf("Auth attempt for %s from %s at %s", u, h, iso)
	case "open":
		return fmt.Sprintf("SSH session: %s from %s", u, h),
			fmt.Sprintf("Session opened for %s from %s tty=%s at %s", u, h, td, iso)
	}
	if duration != nil {
		d := durationText(*duration)
		return fmt.Sprintf("SSH session closed: %s from %s after %s", u, h, d),
			fmt.Sprintf("Session closed for %s from %s after %s at %s", u, h, d, iso)
	}
	return fmt.Sprintf("SSH session closed: %s from %s", u, h),
		fmt.Sprintf("Session closed for %s from %s at %s", u, h, iso)
}

func busAddress(uid int) string {
	for _, p := range []string{
		fmt.Sprintf("/run/user/%d/dbus/user_bus_socket", uid),
		fmt.Sprintf("/run/user/%d/bus", uid),
	} {
		if _, err := os.Stat(p); err == nil {
			return "unix:path=" + p
		}
	}
	return fmt.Sprintf("unix:path=/run/user/%d/dbus/user_bus_socket", uid)
}

func jsonArg(v any) (string, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return "", err
	}
	return string(b), nil
}

// gdbus wants a{sv} in GVariant text: a JSON object with each value wrapped as a variant.
func gvariantDict(m map[string]any) (string, error) {
	parts := make([]string, 0, len(m))
	for k, v := range m {
		key, err := json.Marshal(k)
		if err != nil {
			return "", err
		}
		val, err := json.Marshal(v)
		if err != nil {
			return "", err
		}
		parts = append(parts, string(key)+": <"+string(val)+">")
	}
	return "{" + strings.Join(parts, ", ") + "}", nil
}

func notify(uid int, summary, body string) {
	sumArg, err := jsonArg(summary)
	if err != nil {
		return
	}
	bodyArg, err := jsonArg(body)
	if err != nil {
		return
	}
	actions, err := jsonArg([]string{"default", "Open"})
	if err != nil {
		return
	}
	hints, err := gvariantDict(map[string]any{
		"x-nemo-preview-summary":       summary,
		"x-nemo-preview-body":          body,
		"x-nemo-remote-action-default": remoteAction,
		"x-nemo-max-content-lines":     5,
		"desktop-entry":                "sshnotif",
	})
	if err != nil {
		return
	}
	cmd := exec.Command("gdbus", "call", "--session",
		"--dest", "org.freedesktop.Notifications",
		"--object-path", "/org/freedesktop/Notifications",
		"--method", "org.freedesktop.Notifications.Notify",
		"sshnotif", "0", iconPath,
		sumArg, bodyArg,
		actions,
		hints,
		"0",
	)
	cmd.Env = append(os.Environ(), "DBUS_SESSION_BUS_ADDRESS="+busAddress(uid))
	cmd.Stdout = io.Discard
	cmd.Stderr = io.Discard
	_ = cmd.Start()
}
