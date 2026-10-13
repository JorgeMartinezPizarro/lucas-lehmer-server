package main

import (
	"encoding/json"
	"math/big"
	"net/http"
	"net/http/httptest"
	"reflect"
	"strings"
	"testing"
)

// referenceLLT is the textbook test on math/big, used to check lucasLehmer.
func referenceLLT(p uint32) bool {
	if p == 2 {
		return true
	}
	if p < 2 {
		return false
	}
	m := new(big.Int).Lsh(big.NewInt(1), uint(p))
	m.Sub(m, big.NewInt(1))
	s := big.NewInt(4)
	for i := uint32(0); i < p-2; i++ {
		s.Mul(s, s)
		s.Sub(s, big.NewInt(2))
		s.Mod(s, m)
	}
	return s.Sign() == 0
}

func TestLucasLehmerMatchesReference(t *testing.T) {
	for p := uint32(0); p <= 1500; p++ {
		if got, want := lucasLehmer(p), referenceLLT(p); got != want {
			t.Errorf("lucasLehmer(%d) = %v, want %v", p, got, want)
		}
	}
}

func TestLucasLehmerKnownExponents(t *testing.T) {
	for _, p := range []uint32{2203, 2281, 3217, 4253, 4423, 9689, 9941} {
		if !lucasLehmer(p) {
			t.Errorf("lucasLehmer(%d) = false, want true", p)
		}
	}
	for _, p := range []uint32{2207, 3203, 4447, 9697} {
		if lucasLehmer(p) {
			t.Errorf("lucasLehmer(%d) = true, want false", p)
		}
	}
}

func post(t *testing.T, path, body string) *httptest.ResponseRecorder {
	t.Helper()
	rec := httptest.NewRecorder()
	newMux().ServeHTTP(rec, httptest.NewRequest(http.MethodPost, path, strings.NewReader(body)))
	return rec
}

func TestLucasLehmerHandler(t *testing.T) {
	rec := post(t, "/lucas-lehmer-test", `{"numbers": [7, "13", 11, 0, 1, 4, 7], "num_threads": 2}`)
	if rec.Code != http.StatusOK {
		t.Fatalf("status = %d, body %s", rec.Code, rec.Body)
	}

	var got struct{ Results []result }
	if err := json.Unmarshal(rec.Body.Bytes(), &got); err != nil {
		t.Fatal(err)
	}
	want := []result{{13, true}, {11, false}, {7, true}, {7, true}, {4, false}, {1, false}, {0, false}}
	if !reflect.DeepEqual(got.Results, want) {
		t.Errorf("results = %v, want %v", got.Results, want)
	}

	if body := post(t, "/lucas-lehmer-test", `{"numbers": []}`).Body.String(); strings.TrimSpace(body) != `{"results":[]}` {
		t.Errorf("empty list body = %s", body)
	}
}

func TestLucasLehmerHandlerRejectsInvalidInput(t *testing.T) {
	for _, body := range []string{
		`not json`,
		`{}`,
		`{"numbers": "3,5"}`,
		`{"numbers": [-7]}`,
		`{"numbers": [7.5]}`,
		`{"numbers": [true]}`,
		`{"numbers": [null]}`,
		`{"numbers": ["abc"]}`,
		`{"numbers": [2147483648]}`,
		`{"numbers": [7], "num_threads": 0}`,
		`{"numbers": [7], "num_threads": "4"}`,
	} {
		if rec := post(t, "/lucas-lehmer-test", body); rec.Code != http.StatusBadRequest {
			t.Errorf("%s: status = %d, want 400", body, rec.Code)
		}
	}
}
