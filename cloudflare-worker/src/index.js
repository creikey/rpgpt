import { OpenAI } from "openai";

export default {
	async fetch(request, env, ctx) {
		const requestBody = await request.json();

		const responseObject = {
			"error": "", // I don't set the error here, because I let server errors become errors in the cloudflare dash. Seems easier to manage
			"rate_limited": false,
		}

		if (true) {
			// mock the response
			responseObject["ai_response"] = [
				{
					"action": "say_to",
					"arguments": ["The Player", "Hey what's going on. bla bla bla this is some testing text. Really I'm quite surprised about how long I'm speaking for."],
				},
				{
					"action": "none",
					"arguments": [],
				}
			];
		} else {
			const openai = new OpenAI({
				apiKey: env.OPENAI_KEY, // defaults to process.env["OPENAI_API_KEY"]
			});
			const chatCompletion = await openai.chat.completions.create({
				messages: requestBody,
				model: 'gpt-3.5-turbo',
			});
			try {
				responseObject["ai_response"] = JSON.parse(chatCompletion.choices[0].message.content);
			} catch (e) {
				responseObject["ai_error"] = "Failed to parse what you output: " + e.message;
			}
		}

		const json = JSON.stringify(responseObject, null, 2);

		return new Response(json, {
			headers: {
				"content-type": "application/json;charset=UTF-8",
			},
		});
	},
};
