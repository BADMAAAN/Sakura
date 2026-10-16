#include <display/tft.h>

class QRcode
{
	private:
		tft_display *tft;
		void render(int x, int y, int color);

	public:
		QRcode(tft_display *display);
		void init();
		static constexpr unsigned maxPayloadBytes = 154;
		bool create(String message);
};
