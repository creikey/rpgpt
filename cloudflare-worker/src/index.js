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
					"action": "none",
				}
				/*
				{
					"action": "say_to",
					"arguments": [
						"The Player",
						"Whoa there, partner! Be careful with that revolver. It's got a real mean streak, ya know?"
					]
				},
				{
					"action": "say_to",
					"arguments": [
						"The Player",
						"I reckon we can talk it out instead. Let's find some whiskey, loosen those nerves."
					]
				},
				{
					"action": "pick_up",
					"arguments": [
						"Whiskey"
					]
				},
				{
					"action": "say_to",
					"arguments": [
						"The Player",
						"Now that we got some whiskey, let's sit down and have a drink. Maybe then we can figure things out, or at least forget 'em."
					]
				}
				*/
			];
			//responseObject["ai_error"] = "Failed to parse what you output: Expected ',' or ']' after array element in JSON at position 124 (line 1 column 125)";
		} else {
			const openai = new OpenAI({
				apiKey: env.OPENAI_KEY,
			});
			const chatCompletion = await openai.chat.completions.create({
				messages: requestBody,
				model: 'gpt-3.5-turbo',
				// model: 'gpt-4-1106-preview'
			});
			const content = chatCompletion.choices[0].message.content;
			try {
				responseObject["ai_response"] = JSON.parse(content);
			} catch (e) {
				console.log(`Ai parse error ${e.message} for its output '${content}`);
				responseObject["ai_error"] = `Failed to parse what you output, '${content}': ${e.message}`;
			}
		}

		const json = JSON.stringify(responseObject, null, 2);

		return new Response(json, {
			headers: {
				"Access-Control-Allow-Origin": "*",
				"content-type": "application/json;charset=UTF-8",
			},
		});
	},
};
