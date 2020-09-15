#include <getopt.h>
#include <cstring>
#include <csignal>
#include <iostream>

#include "progress.h"
#include "http.h"

void show_notification(const char* name) noexcept
{
	std::cerr << "Try '" << name << " --help' for more information." << std::endl;
}

void show_usage(const char* name) noexcept
{

	std::cout << "Usage : " << name << " <URL> [OPTION...]" << std::endl
			  << "-d, --directory      Download directory." << std::endl
			  << "-h, --help           Display this help and exit." << std::endl
			  << "-o, --output         Output file name." << std::endl
			  << "-r, --rewrite        Rewrite if file exists." << std::endl;
}

void handler(int)
{
	http::Progress::cancel();
}

int main (int argc, char* argv[])
{
	char* p = std::strrchr (argv[0], '/');
	const char* progname = (p ? ++p : argv[0]);

    if (argc < 2)
    {
        show_usage(progname);
        return EXIT_SUCCESS;
    }

	struct sigaction sig;
	memset(&sig, 0, sizeof (struct sigaction));

	sig.sa_handler = handler;
	sigemptyset (&sig.sa_mask);
	sig.sa_flags = 0;
#ifdef SA_RESTART
	sig.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */

	if (sigaction (SIGINT, &sig, nullptr) < 0)
	{
		return EXIT_FAILURE;
	}

    std::filesystem::path directory;
    std::filesystem::path file_name;
    bool rewrite = false;

	option longopts[] =
	{
		{ "directory",	required_argument,	NULL, 'd'},
		{ "help",		no_argument,		NULL, 'h'},
		{ "output",		required_argument,	NULL, 'o'},
		{ "rewrite",	no_argument,		NULL, 'r'},
		{ 0, 0, 0, 0 }
	};

	/* Command line option parsing. */
	while (true)
	{
		int index;
		int opt = getopt_long (argc, argv, "d:ho:r", longopts, &index);

		if (opt == EOF)
			break;

		switch (opt)
		{
			case 'd':
			{
				directory = optarg;
				break;
			}

			case 'h':
			{
				show_usage(progname);
				return EXIT_SUCCESS;
			}

			case 'o':
			{
				file_name = optarg;
				break;
			}

			case 'r':
			{
				rewrite = true;
				break;
			}

			default:
			{
				show_notification(progname);
				return EXIT_SUCCESS;
			}
		}
	}

    try
    {
        http::Downloader dowloader(std::make_unique<http::Progress>());
        dowloader.dowload(argv[argc - 1], directory, file_name, rewrite);
    }
    catch (const std::invalid_argument& e)
    {
        std::cerr << e.what() << std::endl;
    }
    catch (const std::domain_error& e)
    {
        std::cerr << e.what() << std::endl;
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}
