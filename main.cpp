namespace {
	extern "C" int namespace_extern(int a) {
		return a * 2;
	}
}

extern "C" {
	namespace {
		int extern_namespace(int a) {
			return a / 2;
		}
	}
}

extern "C" {
	static int extern_static(int a) {
		return a - 2;
	}
}

namespace {
	int namespace_cpp(int a) {
		return a + 2;
	}
}
