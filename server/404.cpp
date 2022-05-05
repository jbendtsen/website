#include "website.h"

void serve_404(Filesystem& fs, Response& response)
{
	response.status = "404 Not Found";

	response.html.add("<!DOCTYPE html><html><head>" HTML_METAS "<title>404 - Page Not Found</title><style>\n");
	fs.add_file_to_html(&response.html, "client/banner.css");
	response.html.add("</style></head><body>");
	add_banner(fs, &response.html, NAV_IDX_NULL);

	response.html.add("<div style=\"margin: auto; text-align: center;\"><h1>404</h1>"
		"<p>The thing you were trying to load was not found!<br>Could be a typo, a bug or something I decided to delete. Oops.</p></div>"
		"</body></html>");
}
