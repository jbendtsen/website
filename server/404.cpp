#include "website.h"

void serve_404(Filesystem& fs, Response& response)
{
	response.status = "404 Not Found";

	response.html.add("<!DOCTYPE html><html><head>"
		"<meta charset=\"UTF-8\">"
		"<title>404 - Page Not Found</title><style>\n");

	fs.add_file_to_html(&response.html, "client/banner.css");

	response.html.add("</style></head><body>");

	add_banner(fs, &response.html, NAV_IDX_NULL);

	response.html.add("<h1>404</h1>"
		"<p>Looks like you're outta luck, bud!</p>"
		"</body></html>");
}
